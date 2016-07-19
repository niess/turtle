/*
 *	Topographic elevation provider
 */

#include <float.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <png.h>

#include "topo.h"
#include "geotiff16.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* Parameters of a frame transform. */
struct transform_data {
	struct topo_box box;
	double dldx;
	double dLdy;
};

/* Container for a local elevation map. */
struct topo_map {
	/* Chained list pointers. */
	struct topo_map * prev;
	struct topo_map * next;

	/* Status. */
	int tick;
	int booking;

	/* Map identifiers. */
	char * path;
	struct transform_data * transform;

	/* Map data. */
	int nx, ny;
	double x0, y0;
	double dx, dy;
	double zmin, zmax;
	double * z;

	char data[]; /* Placeholder for dynamic data. */
};

/* Generic map allocation and initialisation. */
static struct topo_map * map_create(const char * path, const struct
	transform_data * transform, int nx, int ny, int n_extra);
static void map_link(struct topo_map * map, int compute_scale);

/* Low level data loaders/dumpers. */
static struct topo_map * load_gdem2(const char* path,
	const struct topo_box * box);
static struct topo_map * load_png(const char* path,
	const struct topo_box * box);
static int dump_png(const struct topo_map * map, const char * path,
	int bit_depth);

/* Coordinates transform. */
static int transform_initialise(struct transform_data * transform,
	const struct topo_box * box);
static int transform_to_geo(double x, double y, const struct transform_data *
	transform, double * longitude, double * latitude);
static int transform_to_local(double longitude, double latitude,
	const struct transform_data * transform, double * x, double * y);

/* WGS84 reference ellipsoid. */
static const double a_WGS84 = 6378137.000;
static const double b_WGS84 = 6356752.314;

/* Hardcoded ASTER-GDEM2 settings. */
static const double GDEM2_pixelscale = 1./3600.;

/* Entry for local topographic maps. */
static struct topo_map * first_map = NULL;
static struct topo_map * last_map = NULL;
static int n_maps = 0;

/* Initialise the topographer interface. */
int topo_initialise(void)
{
	return Geotiff16_Initialise();
}

/* Clear the topographer interface. BEWARE: due to libxml2 this function must
 * be called only once.
 */
void topo_finalise(void)
{
	/* Clear maps. */
	struct topo_map * map = first_map;
	while(map != NULL) {
		struct topo_map * next = map->next;
		free(map);
		map = next;
	}
	first_map = last_map = NULL;
	n_maps = 0;

	/* Clear geotiff-16 data. */
	Geotiff16_Finalise();

	/* Clear remanent XML paser data. */
	xmlCleanupParser();
}

/* Release the map memory and update the chained list. */
void topo_destroy(struct topo_map ** map)
{
	/* Release the memory. */
	if ((map == NULL) || (*map == NULL)) return;
	struct topo_map * next = (*map)->next, * prev = (*map)->prev;
	free(*map);
	*map = NULL;

	/* Update the chained list. */
	if (prev != NULL) prev->next = next;
	else first_map = next;
	if (next != NULL) next->prev = prev;
	else last_map = prev;
	n_maps--;
}

/* Create a blank map. */
struct topo_map * topo_create(const char * path, const int nx, const int ny)
{
	struct topo_map * map = map_create(path, NULL, nx, ny, 0);
	if (map == NULL) return NULL;

	map->nx = nx;
	map->ny = ny;
	map->x0 = 0.;
	map->y0 = 0.;
	map->dx = 1.;
	map->dy = 1.;
	memset(map->z, 0x0, nx*ny*sizeof(double));

	map_link(map, 0);
	return map;
}

/* Load a map from a data file. */
struct topo_map * topo_load(const char * path, const struct topo_box * box)
{
	struct topo_map * map = NULL;

	/* Load the map data. */
	const char * basename = strrchr(path, '/');
	if (basename == NULL) basename = path;
	else basename++;

	if (strcmp(basename, "ASTER-GDEM2") == 0) {
		/* We have ASTER-GDEM2 data. */
		map = load_gdem2(path, box);
		if (map == NULL) goto error;
	}
	else {
		/* Get the file extension. */
		char* ext = strrchr(basename, '.');
		if (ext == NULL) goto error;
		ext++;

		if (strcmp(ext, "png") == 0) {
			map = load_png(path, box);
			if (map == NULL) goto error;
		}
		else goto error;
	}

	/* Link the map and return. */
	map_link(map, 1);
	return map;

error:
	free(map);
	return NULL;
}

/* Save the map to disk. */
int topo_dump(const struct topo_map * map, const char * path, int bit_depth)
{
	/* Get the file extension */
	char* ext = strrchr(path, '.');
	if (ext == NULL) return -1;
	ext++;

	if (strcmp(ext, "png") == 0) return dump_png(map, path, bit_depth);
	else return -1;
}

/* Get an existing map by a linear search. Returns NULL if the map wasn't
 * found.
 */
struct topo_map * topo_get(const char * path, const struct topo_box * box)
{
	/* Search the map. */
	struct topo_map * map = last_map;
	while(map != NULL) {
		const struct topo_box * m_box = (map->transform == NULL) ?
			NULL : &(map->transform->box);
		int same_box = 0;
		if ((box != NULL) && (m_box != NULL))
			same_box = (box->half_x != m_box->half_x) &&
				(box->half_y != m_box->half_y) &&
				(box->x0 != m_box->x0) &&
				(box->y0 != m_box->y0);
		else
			same_box = (box == NULL) && (m_box == NULL);
		if (same_box) break;
		map = map->prev;
	}

	return map;
}

/* Get the oldest unused map. */
struct topo_map * topo_unused(void)
{
	struct topo_map * map = first_map;
	while(map != NULL) {
		if (map->booking == 0) break;
		map = map->next;
	}
	return map;
}

/* Book an existing map. */
void topo_book(struct topo_map * map)
{
	map->booking++;
}

/* Checkout from a map booking. Tick time is updated. */
int topo_checkout(struct topo_map * map)
{
	/* Update and checkout. */
	if (map->booking <= 0) return -1;
	map->tick = time(NULL);
	map->booking--;

	/* Move the map to the end of the chained list. */
	struct topo_map * next = map->next;
	if (next != NULL) {
		struct topo_map * prev = map->prev;
		next->prev = prev;
		if (prev != NULL) prev->next = next;
		else first_map = next;
		last_map->next = map;
		map->prev = last_map;
		last_map = map;
		map->next = NULL;
	}
	return 0;
}

/* Interpolate the elevation at a given location. */
double topo_elevation(const struct topo_map * map, double x, double y)
{
	double hx = (x-map->x0)/map->dx;
	double hy = (y-map->y0)/map->dy;
	int ix = (int)hx;
	int iy = (int)hy;

	if (ix >= map->nx-1 || ix < 0 || iy >= map->ny-1 || iy < 0)
		return -DBL_MAX;
	hx -= ix;
	hy -= iy;

 	const int nx = map->nx;
	const double* z = map->z;
	return z[iy*nx+ix]*(1.-hx)*(1.-hy)+z[(iy+1)*nx+ix]*(1.-hx)*hy+
		z[iy*nx+ix+1]*hx*(1.-hy)+z[(iy+1)*nx+ix+1]*hx*hy;
}

/* Get the geographic coordinates at a given map location. */
int topo_geoposition(const struct topo_map * map, double x, double y,
	double * longitude, double * latitude)
{
	if (map->transform == NULL) return -1;
	return transform_to_geo(x, y, map->transform, longitude, latitude);
}

/* Get the local map coordinates for a given geo-location. */
int topo_locals(const struct topo_map * map, double longitude,
	double latitude, double * x, double * y)
{
	if (map->transform == NULL) return -1;
	return transform_to_local(longitude, latitude, map->transform, x, y);
}

/* Get some basic information on a map filled in a bounding box. */
void topo_info(const struct topo_map * map, struct topo_box * box)
{
	box->half_x = 0.5*(map->nx-1)*map->dx;
	box->half_y = 0.5*(map->ny-1)*map->dy;
	box->x0 = map->x0+box->half_x;
	box->y0 = map->y0+box->half_y;
}

static struct topo_map * map_create(const char * path, const struct
	transform_data * transform, int nx, int ny, int n_extra)
{
	/* Allocate the map memory. */
	int n_path = (strlen(path)+1+n_extra)/sizeof(double);
	if ((n_path % sizeof(double)) != 0) n_path++;
	n_path *= sizeof(double);
	const int n_transform = (transform == NULL) ? 0 : sizeof(*transform);
	struct topo_map * map = (struct topo_map *)malloc(
		sizeof(struct topo_map)+n_path+n_transform+
		nx*ny*sizeof(double));
	if (map == NULL) return NULL;

	/* Fill the identifiers. */
	char * p = map->data;
	map->path = p;
	p += n_path;
	strcpy(map->path, path);
	if (n_transform) {
		map->transform = (struct transform_data *)p;
		p += n_transform;
		memcpy(map->transform, transform, n_transform);
	}
	map->z = (double *)(p);

	return map;
}

static void map_link(struct topo_map * map, int compute_scale)
{
	/* Fill the map statistics. */
	if (compute_scale) {
		double * z = map->z;
		double zmin = *z, zmax = *z;
		z++;
		int i = 1, n = map->nx*map->ny;
		for (; i < n; i++) {
			double zi = *z;
			if (zi < zmin) zmin = zi;
			else if (zi > zmax) zmax = zi;
			z++;
		}
		map->zmin = zmin;
		map->zmax = zmax;
	}
	else {
		map->zmin = -DBL_MAX;
		map->zmax = DBL_MAX;
	}

	/* Fill the status. */
	map->tick = (int)time(NULL);
	map->booking = 0;

	/* Link the map. */
	map->prev = last_map;
	if (last_map != NULL) last_map->next = map;
	else first_map = map;
	last_map = map;
	map->next = NULL;
}

struct topo_map * load_gdem2(const char* path, const struct topo_box * box)
{
	if (box == NULL) return NULL;

	/* Compute the local to geograhic coordinates transform. */
	struct transform_data transform;
	if (transform_initialise(&transform, box) != 0) return NULL;

	/* Get the data bounding box in geographic coordinates. */
	double Dl, DL;
	if (transform_to_geo(box->half_x, box->half_y, &transform, &Dl, &DL)
		!= 0) return NULL;
	Dl -= box->x0;
	DL -= box->y0;

	/* Initialise the local map. */
	int nx = 2*((int)(Dl/GDEM2_pixelscale)+1)+1;
	int ny = 2*((int)(DL/GDEM2_pixelscale)+1)+1;

	struct topo_map * map = map_create(path, &transform, nx, ny, 32);
	if (map == NULL) return NULL;

	/* Copy the elevation data */
	const double l0 = box->x0-((nx-1)/2)*GDEM2_pixelscale;
	const double L0 = box->y0-((ny-1)/2)*GDEM2_pixelscale;
	const int l0_tile  = (int)l0;
	const int L0_tile  = (int)L0;
	const int l0_index = (int)((l0-l0_tile)/GDEM2_pixelscale);
	const int L0_index = (int)((L0-L0_tile)/GDEM2_pixelscale);

	int ix = 0, iy = 0;
	int l_tile=l0_tile, L_tile=L0_tile, l_index=l0_index, L_index=L0_index;
	const int n_path = strlen(path);
	while ((ix != nx) && (iy != ny)) {
		/* Load the reference tile. */
		if (l_tile >= 180) l_tile = -180;
		const int absl = abs(l_tile);
		const int absL = abs(L_tile);
		if ((absl > 180) || (absL > 89)) {
			free(map);
			return NULL;
		}

		char cl, cL;
		cl = (l_tile >= 0) ? 'E' : 'W';
		cL = (L_tile >= 0) ? 'N' : 'S';
		sprintf(map->path+n_path,
			"/ASTGTM2_%c%02d%c%03d_dem.tif", cL, absL, cl, absl);

		Geotiff16Type_Map* geotiff = Geotiff16_NewMap(map->path);
		if (geotiff == NULL) {
			free(map);
			return NULL;
		}

		/* Copy the relevant tile's data. */
		int jx = (l_index+nx-1-ix);
		if (jx >= 3600) jx = 3599;
		int jy = (L_index+ny-1-iy);
		if (jy >= 3600) jy = 3599;
		const int nl = jx-l_index+1;
		const int nL = jy-L_index+1;

		int ky = L_index;
		for (;ky <= jy; ky++) {
			int16_t* z_src = geotiff->elevation+(ky+1)*3601+
				l_index+1;
			double* z_dst = map->z+(ky-L_index+iy)*nx+ix;
			int kx = 0;
			for (; kx < nl; kx++) {
				*z_dst = (double)(*z_src);
				z_src++;
				z_dst++;
			}
		}
		Geotiff16_DeleteMap(geotiff);

		/* Update the indices */
		iy += nL;

		if (iy == ny) {
			ix += nl;
			if (ix < nx) {
				l_tile++;
				l_index = 0;
				iy = 0;
				L_tile = L0_tile;
				L_index = L0_index;
			}
		}
		else {
			L_tile++;
			L_index = 0;
		}
	}
	*(map->path+n_path) = '\0';

	/* Fill the local map settings. */
	const double longitude = l0_tile+(l0_index+0.5)*GDEM2_pixelscale;
	const double latitude = L0_tile+(L0_index+0.5)*GDEM2_pixelscale;
	transform_to_local(longitude, latitude, map->transform, &(map->x0),
		&(map->y0));
	map->nx = nx;
	map->ny = ny;
	map->dx = GDEM2_pixelscale/map->transform->dldx;
	map->dy = GDEM2_pixelscale/map->transform->dLdy;

	return map;
}

struct topo_map * load_png(const char* path, const struct topo_box * box)
{
	struct topo_map * map = NULL;
	FILE * fid = NULL;
	png_bytep * row_pointers = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	int nx = 0, ny = 0;

	/* Open the file and check the format. */
	fid = fopen(path, "rb");
	if (fid == NULL) goto error;

	char header[8];
	fread(header, 1, 8, fid);
	if (png_sig_cmp((png_bytep)header, 0, 8) != 0) goto error;

	/* initialize libpng containers. */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,
		NULL);
	if (png_ptr == NULL) goto error;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) goto error;
	if (setjmp(png_jmpbuf(png_ptr))) goto error;
	png_init_io(png_ptr, fid);
	png_set_sig_bytes(png_ptr, 8);

	/* Read the header. */
	png_read_info(png_ptr, info_ptr);
	if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_GRAY)
		goto error;
	png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	if ((bit_depth != 8) && (bit_depth != 16)) goto error;
	nx = png_get_image_width(png_ptr, info_ptr);
	ny = png_get_image_height(png_ptr, info_ptr);
	png_read_update_info(png_ptr, info_ptr);

	/* Parse the XML meta data. */
	double x0 = 0., y0 = 0., z0 = 0., dx = 1., dy = 1., dz = 1.;
	png_textp text_ptr;
	int num_text;
	unsigned n_chunks = png_get_text(png_ptr, info_ptr, &text_ptr,
		&num_text);
	if (n_chunks > 0) {
		int i = 0;
		png_text* p = text_ptr;
		for(; i < num_text; i++) {
			const char* key = p->key;
			const char* text = p->text;
			unsigned int length = p->text_length;
			p++;

			xmlDocPtr doc = NULL;
			doc = xmlReadMemory(text, length, "noname.xml", NULL,
				XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
			if (doc == NULL) goto xml_end;
			xmlNode * root = xmlDocGetRootElement(doc);
			if (root == NULL) goto xml_end;
			if (strcmp((const char *)root->name, "topography")
				!= 0) goto xml_end;

			xmlNode * node;
			for (node = root->children; node != NULL;
				node = node->next) if ((node->type ==
				XML_ELEMENT_NODE) && (node->name != NULL) &&
				(node->children != NULL)) {
				const xmlChar* name = node->name;
				double* attribute = NULL;
				if (strlen((const char *)name) != 2) continue;
				if (name[1] == '0') {
					if (name[0] == 'x')
						attribute = &x0;
					else if (name[0] == 'y')
						attribute = &y0;
					else if (name[0] == 'z')
						attribute = &z0;
				}
				else if (name[0] == 'd') {
					if (name[1] == 'x')
						attribute = &dx;
					else if (name[1] == 'y')
						attribute = &dy;
					else if (name[1] == 'z')
						attribute = &dz;
				}
				if (attribute == NULL) continue;

				xmlNode * subnode;
				for (subnode = node->children; subnode != NULL;
					subnode=subnode->next) if (subnode->type
					== XML_TEXT_NODE) {
					sscanf((const char *)subnode->content,
						"%lf", attribute);
					break;
				}
			}
xml_end:
			if (doc != NULL) xmlFreeDoc(doc);
		}
	}

	/* Load the data. */
	row_pointers = calloc(ny, sizeof(png_bytep));
	if (row_pointers == NULL) goto error;
	int i = 0;
	for (; i < ny; i++) {
		row_pointers[i] = malloc(png_get_rowbytes(png_ptr, info_ptr));
		if (row_pointers[i] == NULL) goto error;
	}
	png_read_image(png_ptr, row_pointers);

	/* Compute the bounding box, if requested. */
	int nx0, ny0, dnx, dny;
	if (box == NULL) {
		/* No bounding box, let's use the full map. */
		nx0 = 0;
		ny0 = 0;
		dnx = nx;
		dny = ny;
	}
	else {
		/* Clip the bounding box to the full map. */
		const double xlow = box->x0-box->half_x;
		const double xhigh = box->x0+box->half_x;
		const double ylow = box->y0-box->half_y;
		const double yhigh = box->y0+box->half_y;

		if (xlow <= x0) nx0 = 0;
		else nx0 = (int)((xlow-x0)/dx);
		if (nx0 >= nx) goto error;
		dnx = (int)((xhigh-x0)/dx)+1;
		if (dnx > nx) dnx = nx;
		else if (dnx <= nx0) goto error;
		dnx -= nx0;

		if (ylow <= y0) ny0 = 0;
		else ny0 = (int)((ylow-y0)/dy);
		if (ny0 >= ny) goto error;
		dny = (int)((yhigh-y0)/dy)+1;
		if (dny > ny) dny = ny;
		else if (dny <= ny0) goto error;
		dny -= ny0;
	}

	/* Allocate the map and copy the data. */
	map = map_create(path, NULL, nx, ny, 0);
	if (map == NULL) goto error;

	double * z = map->z;
	for (i = ny-ny0-1; i >= ny-ny0-dny; i--) {
		png_bytep ptr = row_pointers[i]+nx0*(bit_depth/8);
		int j = 0;
		for (; j < dnx; j++) {
			if (bit_depth == 8) {
				*z = ((double)*ptr)*dz+z0;
				z++;
				ptr += 1;
			}
			else {
				*z = ((double)ntohs(*((uint16_t*)ptr)))*dz+z0;
				z++;
				ptr += 2;
			}
		}
	}

	map->nx = dnx;
	map->ny = dny;
	map->x0 = x0+nx0*dx;
	map->y0 = y0+ny0*dy;
	map->dx = dx;
	map->dy = dy;

	goto exit;
error:
	free(map);
	map = NULL;

exit:
	if (fid != NULL) fclose(fid);
	if (row_pointers != NULL) {
		int i;
		for (i = 0; i < ny; i++) free(row_pointers[i]);
		free(row_pointers);
	}
	png_structpp pp_p = (png_ptr != NULL) ? &png_ptr : NULL;
	png_infopp pp_i = (info_ptr != NULL) ? &info_ptr : NULL;
	png_destroy_read_struct(pp_p, pp_i, NULL);

	return map;
}

/* Dump a map in png format. */
int dump_png(const struct topo_map * map, const char * path, int bit_depth)
{
	int rc = 0;
	FILE* fid = NULL;
	png_bytep * row_pointers = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	/* Check the bit depth. */
	if ((bit_depth != 8) && (bit_depth != 16)) goto error;

	/* Compute the z-binning. */
	const int nx = map->nx, ny = map->ny;
	double z0 = map->zmin, z1 = map->zmax;
	if ((z0 == -DBL_MAX) || (z1 == DBL_MAX)) {
		const double* z = map->z;
		z0 = z1 = *z;
		z++;
		int i = 1, n = nx*ny;
		for (; i < n; i++) {
			double zi = *z;
			if (zi > z1) z1 = zi;
			else if (zi < z0) z0 = zi;
			z++;
		}
	}
	const int bins = (bit_depth == 8 ? 255 : 65535);
	const double dz = (z1-z0)/bins;

	/* Initialise the file and the PNG pointers. */
	fid = fopen(path, "wb+");
	if (fid == NULL) goto error;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,
		NULL);
	if (png_ptr == NULL) goto error;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) goto error;
	if (setjmp(png_jmpbuf(png_ptr))) goto error;
	png_init_io(png_ptr, fid);

	/* Write the header. */
	png_set_IHDR(
		png_ptr, info_ptr, nx, ny, bit_depth, PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE
	);
	char header[1024];
	sprintf(header, "<topography><x0>%.3lf</x0><y0>%.3lf</y0>"
		"<z0>%.3lf</z0><dx>%.3lf</dx><dy>%.3lf</dy>"
		"<dz>%.5e</dz></topography>",
		map->x0, map->y0, z0, map->dx, map->dy, dz);
	png_text text[] = {{PNG_TEXT_COMPRESSION_NONE, "Comment", header,
		strlen(header)}};
	png_set_text(png_ptr, info_ptr, text, sizeof(text)/sizeof(text[0]));
	png_write_info(png_ptr, info_ptr);

	/* Write the data */
	row_pointers = (png_bytep*)calloc(ny, sizeof(png_bytep));
	if (row_pointers == NULL) goto error;
	int i = 0;
	for (; i < ny; i++) {
		row_pointers[i] = (png_byte *)malloc((bit_depth/8)*nx*
			sizeof(char));
		if (row_pointers[i] == NULL) goto error;
		const double * z = map->z+(ny-i-1)*nx;
		if (bit_depth == 8) {
			unsigned char* ptr = (unsigned char*)row_pointers[i];
			int j = 0;
			for (; j < nx; j++) {
				*ptr = (unsigned char)((*z-z0)/dz);
				z++;
				ptr++;
			}
		}
		else
		{
			uint16_t* ptr = (uint16_t*)row_pointers[i];
			int j = 0;
			for (; j < nx; j++) {
				*ptr = htons((uint16_t)((*z-z0)/dz));
				z++;
				ptr++;
			}
		}
	}
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, NULL);

	goto exit;
error:
	rc = -1;

exit:
	if (fid != NULL)
		fclose(fid);
	if (row_pointers != NULL) {
		int i;
		for (i = 0; i < ny; i++) free(row_pointers[i]);
		free(row_pointers);
	}
	png_structpp pp_p = (png_ptr != NULL) ? &png_ptr : NULL;
	png_infopp pp_i = (info_ptr != NULL) ? &info_ptr : NULL;
	png_destroy_write_struct(pp_p, pp_i);

	return rc;
}

/* Set the transform between local and geographic coordinates. */
int transform_initialise(struct transform_data * transform,
	const struct topo_box *  box)
{
	if ((fabs(box->y0) > 90.) || (fabs(box->x0) > 180.))
		goto error;

	const double deg = M_PI/180.;
	const double latitude = box->y0*deg;
	const double a = a_WGS84*cos(latitude);
	const double b = b_WGS84*sin(latitude);
	const double dxdl = a*deg;
	const double dydL = sqrt(a*a+b*b)*deg;

	if ((dxdl <= 0.) || (dydL <= 0.)) goto error;

	memcpy(&(transform->box), box, sizeof(*box));
	transform->dldx = 1./dxdl;
	transform->dLdy = 1./dydL;
	return 0;
error:
	transform->dldx = transform->dLdy = 0.;
	transform->box.x0 = transform->box.y0 = 0.;
	transform->box.half_x = transform->box.half_y = 0.;

	return -1;
}

/* Convert local coordinates to geographic ones. */
int transform_to_geo(double x, double y, const struct transform_data *
	transform, double * longitude, double * latitude)
{
	const double dldx = transform->dldx;
	const double dLdy = transform->dLdy;

	if ((dldx <= 0.) || (dLdy <= 0.)) {
		*longitude = 0.;
		*latitude = 0.;
		return -1;
	}
	else {
		*longitude = x*dldx+transform->box.x0;
		*latitude = y*dLdy+transform->box.y0;
		return 0;
	}
}

/* Convert geographic coordinates to local ones. */
int transform_to_local(double longitude, double latitude, const struct
	transform_data * transform, double * x, double * y)
{
	const double dldx = transform->dldx;
	const double dLdy = transform->dLdy;

	if ((dldx <= 0.) || (dLdy <= 0.)) goto error;
	else if ((fabs(latitude) > 90.) || (fabs(longitude) > 180.)) goto error;
	else {
		*x = (longitude-transform->box.x0)/dldx;
		*y = (latitude-transform->box.y0)/dLdy;
	}
	return 0;

error:
	*x = 0.;
	*y = 0.;
	return -1;
}

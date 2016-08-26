/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
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

#include "turtle.h"
#include "geotiff16.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

enum projection_type {
	PROJECTION_NONE = -1,
	PROJECTION_LAMBERT93,
	PROJECTION_UTM,
	N_PROJECTIONS
};

typedef int (* map_locator)(struct turtle_map * map, double x, double y,
	double * latitude, double * longitude);

/* Container for a projection map. */
struct turtle_map {
	/* Map data. */
	int bit_depth;
	int nx, ny;
	double x0, y0, z0;
	double dx, dy, dz;
	void * z;
	
	/* Projection. */
	enum projection_type projection;
	void * projection_data;

	char data[]; /* Placeholder for dynamic data. */
};

/* Generic map allocation and initialisation. */
static struct turtle_map * map_create(int nx, int ny, int bit_depth,
	enum projection_type projection);

/* Low level data loaders/dumpers. */
static struct turtle_map * map_load_gdem2(const char * datum_path,
	const char * projection, const struct turtle_box * box);
static enum turtle_return map_load_png(const char * path,
	const struct turtle_box * box, struct turtle_map ** map);
static enum turtle_return map_dump_png(const struct turtle_map * map,
	const char * path);
	
/* Routines for map projections. */
enum projection_type projection_index(const char * name);

/* WGS84 reference ellipsoid. */
static const double a_WGS84 = 6378137.000;
static const double b_WGS84 = 6356752.314;

/* Hardcoded ASTER-GDEM2 settings. */
static const double GDEM2_pixelscale = 1./3600.;

/* Initialise the TURTLE interface. */
enum turtle_return turtle_initialise(void)
{
	return (Geotiff16_Initialise() == 0) ? TURTLE_RETURN_SUCCESS :
		TURTLE_RETURN_GEOTIFF_ERROR;
}

/* Clear the TURTLE interface. BEWARE: due to libxml2 this function must
 * be called only once.
 */
void turtle_finalise(void)
{
	/* Clear geotiff-16 data. */
	Geotiff16_Finalise();

	/* Clear remanent XML paser data. */
	xmlCleanupParser();
}

const char * turtle_strerror(enum turtle_return rc)
{
	static const char * msg[N_TURTLE_RETURNS] = {
		"Operation succeeded",
		"Bad file extension",
		"Bad file format",
		"No such file or directory",
		"Unknown projection",
		"Bad XML header",
		"Value is out of bound",
		"Couldn't initialise geotiff",
		"Not enough memory"
	};
	
	if ((rc < 0) || (rc >= N_TURTLE_RETURNS)) return NULL;
	else return msg[rc];
}

/* Release the map memory. */
void turtle_map_destroy(struct turtle_map ** map)
{
	/* Release the memory. */
	if ((map == NULL) || (*map == NULL)) return;
	free(*map);
	*map = NULL;
}

/* Get the file extension in a path. */
const char * path_extension(const char * path)
{
	const char * ext = strrchr(path, '.');
	if (ext != NULL) ext++;
	return ext;
}

/* Load a map from a data file. */
enum turtle_return turtle_map_load(const char * path,
	const struct turtle_box * box, struct turtle_map ** map)
{
	/* Check the file extension. */
	const char * ext = path_extension(path);
	if (ext == NULL) return TURTLE_RETURN_BAD_EXTENSION;

	if (strcmp(ext, "png") == 0)
		return map_load_png(path, box, map);
	else
		return TURTLE_RETURN_BAD_EXTENSION;
}

/* Save the map to disk. */
enum turtle_return turtle_map_dump(const struct turtle_map * map,
	const char * path)
{
	/* Check the file extension. */
	const char * ext = path_extension(path);
	if (ext == NULL) return TURTLE_RETURN_BAD_EXTENSION;

	if (strcmp(ext, "png") == 0)
		return map_dump_png(map, path);
	else
		return TURTLE_RETURN_BAD_EXTENSION;
}

/* Interpolate the elevation at a given location. */
enum turtle_return turtle_map_elevation(const struct turtle_map * map,
	double x, double y, double * z)
{
	double hx = (x-map->x0)/map->dx;
	double hy = (y-map->y0)/map->dy;
	int ix = (int)hx;
	int iy = (int)hy;

	if (ix >= map->nx-1 || hx < 0 || iy >= map->ny-1 || hy < 0)
		return TURTLE_RETURN_DOMAIN_ERROR;
	hx -= ix;
	hy -= iy;

 	const int nx = map->nx;
 	if (map->bit_depth == 8) {
		const unsigned char * zm = map->z;
		*z = zm[iy*nx+ix]*(1.-hx)*(1.-hy)+zm[(iy+1)*nx+ix]*(1.-hx)*hy+
			zm[iy*nx+ix+1]*hx*(1.-hy)+zm[(iy+1)*nx+ix+1]*hx*hy;
	}
	else {
		const uint16_t * zm = map->z;
		*z = zm[iy*nx+ix]*(1.-hx)*(1.-hy)+zm[(iy+1)*nx+ix]*(1.-hx)*hy+
			zm[iy*nx+ix+1]*hx*(1.-hy)+zm[(iy+1)*nx+ix+1]*hx*hy;
	}
	*z = (*z)*map->dz+map->z0;
	return TURTLE_RETURN_SUCCESS;
}

/* Get some basic information on a map. */
void turtle_info(const struct turtle_map * map, struct turtle_box * box,
	double * zmin, double * zmax)
{
	if (box != NULL) {
		box->half_x = 0.5*(map->nx-1)*map->dx;
		box->half_y = 0.5*(map->ny-1)*map->dy;
		box->x0 = map->x0+box->half_x;
		box->y0 = map->y0+box->half_y;
	}
	if (zmin != NULL) *zmin = map->z0;
	if (zmax != NULL) *zmax = map->z0+pow(2, map->bit_depth)*map->dz;
}

static struct turtle_map * map_create(int nx, int ny, int bit_depth,
	enum projection_type projection)
{
	/* Allocate the map memory. */
	int datum_size;
	if (bit_depth == 8) datum_size = sizeof(unsigned char);
	else datum_size = sizeof(uint16_t);
	struct turtle_map * map = malloc(sizeof(*map)+nx*ny*datum_size);
	if (map == NULL) return NULL;

	/* Fill the identifiers. */
	map->nx = nx;
	map->ny = ny;
	map->bit_depth = bit_depth;
	map->projection = projection;
	char * p = map->data;
	map->z = (void *)(p);
	map->projection_data = NULL;

	return map;
}

enum projection_type projection_index(const char * name)
{
	static const char * names[N_PROJECTIONS] = {
		"Lambert93",
		"UTM"
	};
	
	int i;
	for (i = 0; i < N_PROJECTIONS; i++)
		if (strcmp(name, names[i]) == 0) return i;
	return PROJECTION_NONE;
}

const char * get_node_content(xmlNode * node)
{
	xmlNode * subnode;
	for (subnode = node->children; subnode != NULL;
		subnode=subnode->next) if (subnode->type == XML_TEXT_NODE) {
		return (const char *)subnode->content;
	}
	return NULL;
}

enum turtle_return map_load_png(const char* path, const struct turtle_box * box,
	struct turtle_map ** map)
{
	FILE * fid = NULL;
	png_bytep * row_pointers = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	int nx = 0, ny = 0;
	enum projection_type projection = PROJECTION_NONE;
	*map = NULL;
	int rc;

	/* Open the file and check the format. */
	fid = fopen(path, "rb");
	if (fid == NULL) {
		rc = TURTLE_RETURN_BAD_PATH;
		goto error;
	}

	char header[8];
	rc = TURTLE_RETURN_BAD_FORMAT;
	if (fread(header, 1, 8, fid) != 8) goto error;
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
	rc = TURTLE_RETURN_BAD_XML;
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
				const char * name = (const char *)node->name;
				if (strcmp(name, "projection") == 0) {
					const char * content =
						get_node_content(node);
					if (content == NULL) goto error;
					projection = projection_index(content);
					if (projection == PROJECTION_NONE) {
						rc = TURTLE_RETURN_BAD_PROJECTION;
						goto error;
					}
					
				}
				double * attribute = NULL;
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

				const char * content = get_node_content(node);
				if (content == NULL) goto error;
				sscanf(content, "%lf", attribute);
			}
xml_end:
			if (doc != NULL) xmlFreeDoc(doc);
		}
	}

	/* Load the data. */
	rc = TURTLE_RETURN_MEMORY_ERROR;
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

		rc = TURTLE_RETURN_DOMAIN_ERROR;
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
	*map = map_create(nx, ny, bit_depth, projection);
	if (*map == NULL) {
		rc = TURTLE_RETURN_MEMORY_ERROR;
		goto error;
	}

	unsigned char * z8 = (*map)->z;
	uint16_t * z16 = (*map)->z;
	for (i = ny-ny0-1; i >= ny-ny0-dny; i--) {
		png_bytep ptr = row_pointers[i]+nx0*(bit_depth/8);
		int j = 0;
		for (; j < dnx; j++) {
			if (bit_depth == 8) {
				*z8 = (unsigned char)*ptr;
				z8++;
				ptr += 1;
			}
			else {
				*z16 = (uint16_t)ntohs(*((uint16_t *)ptr));
				z16++;
				ptr += 2;
			}
		}
	}

	(*map)->nx = dnx;
	(*map)->ny = dny;
	(*map)->x0 = x0+nx0*dx;
	(*map)->y0 = y0+ny0*dy;
	(*map)->z0 = z0;
	(*map)->dx = dx;
	(*map)->dy = dy;
	(*map)->dz = dz;

	goto exit;
error:
	free(*map);
	*map = NULL;

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

	return TURTLE_RETURN_SUCCESS;
}

/* Dump a map in png format. */
enum turtle_return map_dump_png(const struct turtle_map * map,
	const char * path)
{
	FILE* fid = NULL;
	png_bytep * row_pointers = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	enum turtle_return rc;

	/* Check the bit depth. */
	rc = TURTLE_RETURN_BAD_FORMAT;
	if ((map->bit_depth != 8) && (map->bit_depth != 16)) goto exit;

	/* Initialise the file and the PNG pointers. */
	rc = TURTLE_RETURN_BAD_PATH;
	fid = fopen(path, "wb+");
	if (fid == NULL) goto exit;

	rc = TURTLE_RETURN_MEMORY_ERROR;
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,
		NULL);
	if (png_ptr == NULL) goto exit;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) goto exit;
	if (setjmp(png_jmpbuf(png_ptr))) goto exit;
	png_init_io(png_ptr, fid);

	/* Write the header. */
	const int nx = map->nx, ny = map->ny;
	png_set_IHDR(
		png_ptr, info_ptr, nx, ny, map->bit_depth, PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
		PNG_FILTER_TYPE_BASE
	);
	char header[1024];
	sprintf(header, "<topography><x0>%.3lf</x0><y0>%.3lf</y0>"
		"<z0>%.3lf</z0><dx>%.3lf</dx><dy>%.3lf</dy>"
		"<dz>%.5e</dz></topography>",
		map->x0, map->y0, map->z0, map->dx, map->dy, map->dz);
	png_text text[] = {{PNG_TEXT_COMPRESSION_NONE, "Comment", header,
		strlen(header)}};
	png_set_text(png_ptr, info_ptr, text, sizeof(text)/sizeof(text[0]));
	png_write_info(png_ptr, info_ptr);

	/* Write the data */
	row_pointers = (png_bytep*)calloc(ny, sizeof(png_bytep));
	if (row_pointers == NULL) goto exit;
	int i = 0;
	for (; i < ny; i++) {
		row_pointers[i] = (png_byte *)malloc((map->bit_depth/8)*nx*
			sizeof(char));
		if (row_pointers[i] == NULL) goto exit;
		if (map->bit_depth == 8) {
			unsigned char * ptr = (unsigned char*)row_pointers[i];
			const unsigned char * z = (const unsigned char *)
				(map->z)+(ny-i-1)*nx;
			int j = 0;
			for (; j < nx; j++) {
				*ptr = *z;
				z++;
				ptr++;
			}
		}
		else
		{
			uint16_t * ptr = (uint16_t*)row_pointers[i];
			const uint16_t * z = (const uint16_t  *)(map->z)+
				(ny-i-1)*nx;
			int j = 0;
			for (; j < nx; j++) {
				*ptr = (uint16_t)htons(*z);
				z++;
				ptr++;
			}
		}
	}
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, NULL);
	rc = TURTLE_RETURN_SUCCESS;
	
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

#if(0)
/* Create a new datum. */
struct turtle_datum * turtle_datum_create(const char * path,
	int stack_size, turtle_datum_cb lock, turtle_datum_cb release)
{
	/* Load the map data. */
	const char * basename = strrchr(path, '/');
	if (basename == NULL) basename = path;
	else basename++;

	if (strcmp(basename, "ASTER-GDEM2") == 0) {
		/* We have ASTER-GDEM2 data. */
		return NULL; /* TODO */
	}
	else {
		/* Other datums are not yet supported ... */
		return NULL;
	}
}

/* Create a projection map from a datum. */
struct turtle_map * turtle_datum_project(struct turtle_datum * datum,
	const char * projection, const struct turtle_box * box)
{
	/* return load_gdem2(path, box); */
}

struct turtle_map * load_gdem2(const char* path, const struct turtle_box * box)
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

	struct turtle_map * map = map_create(path, &transform, nx, ny, 32);
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
#endif

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
	PROJECTION_LAMBERT,
	PROJECTION_UTM,
	N_PROJECTIONS
};

typedef int (* map_locator)(struct turtle_map * map, double x, double y,
	double * latitude, double * longitude);

/* Data storage for projection settings. */
union projection_settings {
	struct {
		double longitude_0;
		int hemisphere;
	} utm;
	int lambert_tag;
};

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
	union projection_settings settings;

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
static enum turtle_return projection_parse(const char * name,
	enum projection_type * projection,
	union projection_settings * settings);
static enum turtle_return project_lambert(const struct turtle_map * map,
	double latitude, double longitude, double * x, double * y);
static enum turtle_return unproject_lambert(const struct turtle_map * map,
	double x, double y, double * latitude, double * longitude);
static enum turtle_return project_utm(const struct turtle_map * map,
	double latitude, double longitude, double * x, double * y);
static enum turtle_return unproject_utm(const struct turtle_map * map,
	double x, double y, double * latitude, double * longitude);

/* Hardcoded ASTER-GDEM2 settings. */
static const double GDEM2_pixelscale = 1./3600.;

/* Initialise the TURTLE interface. */
enum turtle_return turtle_initialise(void)
{
	/* Initialise the libxml2 parser. */
	xmlInitParser();

	/* Initialise GEOTIFF-16. */
	return (Geotiff16_Initialise() == 0) ? TURTLE_RETURN_SUCCESS :
		TURTLE_RETURN_GEOTIFF_ERROR;
}

/* Clear the TURTLE interface. BEWARE: due to libxml2 this function must
 * be called only once.
 */
void turtle_finalise(void)
{
	/* Clear GEOTIFF-16 data. */
	Geotiff16_Finalise();

	/* Clear remanent XML paser data. */
	xmlCleanupParser();
}

const char * turtle_strerror(enum turtle_return rc)
{
	static const char * msg[N_TURTLE_RETURNS] = {
		"Operation succeeded",
		"Bad address",
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

/* Project geodetic coordinates on the map. */
enum turtle_return turtle_map_project(const struct turtle_map * map,
	double latitude, double longitude, double * x, double * y)
{
	*x = 0.;
	*y = 0.;

	if (map == NULL)
		return TURTLE_RETURN_BAD_ADDRESS;
	else if (map->projection == PROJECTION_NONE)
		return TURTLE_RETURN_BAD_PROJECTION;
	else if (map->projection == PROJECTION_LAMBERT)
		return project_lambert(map, latitude, longitude, x, y);
	else
		return project_utm(map, latitude, longitude, x, y);
}

/* Unproject map coordinates to geodetic ones. */
enum turtle_return turtle_map_unproject(const struct turtle_map * map,
	double x, double y, double * latitude, double * longitude)
{
	*latitude = 0.;
	*longitude = 0.;

	if (map == NULL)
		return TURTLE_RETURN_BAD_ADDRESS;
	else if (map->projection == PROJECTION_NONE)
		return TURTLE_RETURN_BAD_PROJECTION;
	else if (map->projection == PROJECTION_LAMBERT)
		return unproject_lambert(map, x, y, latitude, longitude);
	else
		return unproject_utm(map, x, y, latitude, longitude);
}

/* Get some basic information on a map. */
enum turtle_return turtle_map_info(const struct turtle_map * map,
	struct turtle_box * box, double * zmin, double * zmax,
	char ** projection)
{
	if (box != NULL) {
		box->half_x = 0.5*(map->nx-1)*map->dx;
		box->half_y = 0.5*(map->ny-1)*map->dy;
		box->x0 = map->x0+box->half_x;
		box->y0 = map->y0+box->half_y;
	}
	if (zmin != NULL) *zmin = map->z0;
	if (zmax != NULL) *zmax = map->z0+pow(2, map->bit_depth)*map->dz;

	if (projection == NULL) return TURTLE_RETURN_SUCCESS;

	*projection = NULL;
	if (map->projection == PROJECTION_NONE) return TURTLE_RETURN_SUCCESS;

	enum turtle_return rc = TURTLE_RETURN_MEMORY_ERROR;
	int size, nw = 0;
	for (size = 128;; size += 128) {
		char * tmp = realloc(*projection, size);
		if (tmp == NULL) goto error;
		*projection = tmp;

		if (map->projection == PROJECTION_LAMBERT) {
			const char * tag[6] = {"I", "II", "IIe", "III", "IV",
				"93"};
			nw = snprintf(*projection, size, "Lambert %s",
				tag[map->settings.lambert_tag]);
			if (nw < size) break;
		}
		else if (map->projection == PROJECTION_UTM) {
			char hemisphere = (map->settings.utm.hemisphere > 0) ?
				'N' : 'S';
			double tmp = map->settings.utm.longitude_0/6.+183.;
			int zone = (int)tmp;
			if (fabs(tmp-zone) <= FLT_EPSILON)
				nw = snprintf(*projection, size, "UTM %d%c",
					zone, hemisphere);
			else
				nw = snprintf(*projection, size, "UTM %.12lg%c",
					zone, hemisphere);
			if (nw < size) break;
		}
		else {
			rc = TURTLE_RETURN_BAD_PROJECTION;
			goto error;
		}
	}

	nw++;
	if (nw < size) *projection = realloc(*projection, nw);
	return TURTLE_RETURN_SUCCESS;

error:
	free(*projection);
	*projection = NULL;
	return rc;
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

	return map;
}


int locate_word(const char ** str)
{
	const char * p = *str;
	while (*p == ' ') p++;
	*str = p;
	int n = 0;
	while ((*p != ' ') && (*p != '\0')) { p++; n++; }
	return n;
}

enum turtle_return projection_parse(const char * name,
	enum projection_type * projection, union projection_settings * settings)
{
	*projection = PROJECTION_NONE;
	if (name == NULL) return TURTLE_RETURN_BAD_PROJECTION;

	/* Locate the 1st word. */
	const char * p = name;
	int n = locate_word(&p);

	if (n == 0) {
		/* No projection has been defined. */
		return TURTLE_RETURN_SUCCESS;
	}
	else if (strncmp(p, "Lambert", n) == 0) {
		/* This is a Lambert projection. Let's parse the parameters
		 * set.
		 */
		*projection = PROJECTION_LAMBERT;
		p += n;
		n = locate_word(&p);
		const char * tag[6] = {"I", "II", "IIe", "III", "IV", "93"};
		int i;
		for (i = 0; i < 6; i++) {
			if (strncmp(p, tag[i], n) == 0) {
				settings->lambert_tag = i;
				return TURTLE_RETURN_SUCCESS;
			}

		}
	}
	else if (strncmp(p, "UTM", n) == 0) {
		/* This is a UTM projection. Let's parse the parameters
		 * set.
		 */
		*projection = PROJECTION_UTM;
		p += n;
		int zone;
		char hemisphere;
		if (sscanf(p, "%d%c", &zone, &hemisphere) != 2)
			return TURTLE_RETURN_BAD_PROJECTION;
		if (hemisphere == '.') {
			double longitude_0;
			if (sscanf(p, "%lf%c", &longitude_0,
				&hemisphere) != 2)
				return TURTLE_RETURN_BAD_PROJECTION;
			settings->utm.longitude_0 = longitude_0;
		}
		else
			settings->utm.longitude_0 = 6.*zone-183.;
		if (hemisphere == 'N')
			settings->utm.hemisphere = 1;
		else if (hemisphere == 'S')
			settings->utm.hemisphere = -1;
		else
			return TURTLE_RETURN_BAD_PROJECTION;
		return TURTLE_RETURN_SUCCESS;
	}

	return TURTLE_RETURN_BAD_PROJECTION;
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
	union projection_settings settings;
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
					if ((rc = projection_parse(content,
						&projection, &settings)) !=
						TURTLE_RETURN_SUCCESS)
						goto error;

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
	(*map)->projection = projection;
	memcpy(&((*map)->settings), &settings, sizeof(settings));

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

/* Compute the isometric latitude for Lambert projections.
 *
 * Source:
 * 	ALG0001 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
double lambert_latitude_to_iso(double latitude, double e)
{
	const double phi = latitude*M_PI/180.;
	const double s = sin(phi);
	return log(tan(0.25*M_PI+0.5*phi)*pow((1.-e*s)/(1.+e*s), 0.5*e));
}

/* Compute the geographic latitude from the isometric one for Lambert
 * projections.
 *
 * Source:
 * 	ALG0001 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
double lambert_iso_to_latitude(double L, double e)
{
	const double epsilon = FLT_EPSILON;

	const double eL = exp(L);
	double phi0 = 2.*atan(eL)-0.5*M_PI;
	for (;;) {
		const double s = sin(phi0);
		double phi1 = 2.*atan(pow((1.+e*s)/(1.-e*s), 0.5*e)*eL)
			-0.5*M_PI;
		if (fabs(phi1-phi0) <= epsilon) return phi1/M_PI*180.;
		phi0 = phi1;
	}
}

/* Parameters for a Lambert projection. */
struct lambert_parameters {
	double e;
	double n;
	double c;
	double lambda_c;
	double xs;
	double ys;
};

/* Compute the projected coordinates for Lambert projections.
 *
 * Source:
 * 	ALG0003 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
void lambert_ll_to_xy(double latitude, double longitude,
	struct lambert_parameters * parameters, double * x, double * y)
{
	const double L = lambert_latitude_to_iso(latitude, parameters->e);
	const double cenL = parameters->c*exp(-parameters->n*L);
	const double lambda = longitude/180.*M_PI;
	const double theta = parameters->n*(lambda-parameters->lambda_c);
	*x = parameters->xs+cenL*sin(theta);
	*y = parameters->ys-cenL*cos(theta);
}

/* Compute the geodetic coordinates from the projected ones for a Lambert
 * projection.
 *
 * Source:
 * 	ALG0004 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
void lambert_xy_to_ll(double x, double y,
	struct lambert_parameters * parameters, double * latitude,
	double * longitude)
{
	const double dx = x-parameters->xs;
	const double dy = y-parameters->ys;
	const double R = sqrt(dx*dx+dy*dy);
	const double gamma = atan2(dx, -dy);
	*longitude = (parameters->lambda_c+gamma/parameters->n)*180./M_PI;
	const double L = -log(R/parameters->c)/parameters->n;
	*latitude = lambert_iso_to_latitude(L, parameters->e);
}

/* Get the parameters for a specific Lambert projection.
 * Beware: there is no bound check.
 *
 * Source:
 * 	http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 * 	http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	rgf93/Lambert-93.pdf (for Lambert 93/RGF93).
 */
struct lambert_parameters * lambert_get_parameters(int tag)
{
	static struct lambert_parameters parameters[6] = {
		{0.08248325676, 0.7604059656, 11603796.98, 0.04079234433,
			600000.0, 5657616.674},
		{0.08248325676, 0.7289686274, 11745793.39, 0.04079234433,
			600000.0, 6199695.768},
		{0.08248325676, 0.7289686274, 11745793.39, 0.04079234433,
			600000.0, 8199695.768},
		{0.08248325676, 0.6959127966, 11947992.52, 0.04079234433,
			600000.0, 6791905.085},
		{0.08248325676, 0.6712679322, 12136281.99, 0.04079234433,
			234.358, 7239161.542},
		/* Lambert 93/RGF93 have been recomputed from specs. NTG_71
		 * values do not conform to RGF93. */
		{0.08181919112, 0.7253743710, 11755528.70, 0.05235987756,
			700000.0, 12657560.145}
	};

	return parameters+tag;
}

/* Encapsulation of Lambert projections. */
enum turtle_return project_lambert(const struct turtle_map * map,
	double latitude, double longitude, double * x, double * y)
{
	struct lambert_parameters * parameters = lambert_get_parameters(
		map->settings.lambert_tag);
	lambert_ll_to_xy(latitude, longitude, parameters, x, y);
	return TURTLE_RETURN_SUCCESS;
}

/* Encapsulation of inverse Lambert projections.  */
enum turtle_return unproject_lambert(const struct turtle_map * map,
	double x, double y, double * latitude, double * longitude)
{
	struct lambert_parameters * parameters = lambert_get_parameters(
		map->settings.lambert_tag);
	lambert_xy_to_ll(x, y, parameters, latitude, longitude);
	return TURTLE_RETURN_SUCCESS;
}

/* Compute the projected coordinates for UTM projections.
 *
 * Source:
 * 	Wikipedia https://en.wikipedia.org/wiki/
 * 	Universal_Transverse_Mercator_coordinate_system.
 */
void utm_ll_to_xy(double latitude, double longitude, double longitude_0,
	int hemisphere, double * x, double * y)
{
	const double a = 6378.137E+03;
	const double f = 1./298.257223563;
	const double E0 = 5E+05;
	const double N0 = (hemisphere > 0) ? 0. : 1E+07;
	const double k0 = 0.9996;

	const double n = f/(2.-f);
	const double A = a/(1.+n)*(1.+n*n*(0.25+0.0625*n*n));
	const double alpha[3] = {n*(0.5+n*(-2./3.+5./16.*n)),
		n*n*(13./48.-3./5.*n), 61./240.*n*n*n};

	const double c = 2.*sqrt(n)/(1.+n);
	const double s = sin(latitude*M_PI/180.);
	const double t = sinh(atanh(s)-c*atanh(c*s));
	const double dl = (longitude-longitude_0)*M_PI/180.;
	const double zeta = atan2(t, cos(dl));
	const double eta = atanh(sin(dl)/sqrt(1.+t*t));

	double xs = 0., ys = 0.;
	int i;
	for (i = 0; i < 3; i++) {
		xs += alpha[i]*cos(2.*(i+1)*zeta)*sinh(2.*(i+1)*eta);
		ys += alpha[i]*sin(2.*(i+1)*zeta)*cosh(2.*(i+1)*eta);
	}
	*x = E0+k0*A*(eta+xs);
	*y = N0+k0*A*(zeta+ys);
}

/* Compute the geodetic coordinates from the projected ones for a UTM
 * projection.
 *
 * Source:
 * 	Wikipedia https://en.wikipedia.org/wiki/
 * 	Universal_Transverse_Mercator_coordinate_system.
 */
void utm_xy_to_ll(double x, double y, double longitude_0, int hemisphere,
	double * latitude, double * longitude)
{
	const double a = 6378.137E+03;
	const double f = 1./298.257223563;
	const double E0 = 5E+05;
	const double N0 = (hemisphere > 0) ? 0. : 1E+07;
	const double k0 = 0.9996;

	const double n = f/(2.-f);
	const double A = a/(1.+n)*(1.+n*n*(0.25+0.0625*n*n));
	const double beta[3] = {n*(0.5+n*(-2./3.+37./96.*n)),
		n*n*(1./48.+1./15.*n), 17./480.*n*n*n
	};
	const double delta[3] = {n*(2.+n*(-2./3.-2.*n)),
		n*n*(7./3.-8./5.*n), 56./15.*n*n*n
	};

	const double zeta0 = (y-N0)/(k0*A);
	const double eta0 = (x-E0)/(k0*A);
	double zeta = zeta0, eta = eta0;
	int i;
	for (i = 0; i < 3; i++) {
		zeta -= beta[i]*sin(2.*(i+1)*zeta0)*cosh(2.*(i+1)*eta0);
		eta -= beta[i]*cos(2.*(i+1)*zeta0)*sinh(2.*(i+1)*eta0);
	}
	const double chi = asin(sin(zeta)/cosh(eta));
	double s = 0.;
	for (i = 0; i < 3; i++) s += delta[i]*sin(2.*(i+1)*chi);
	*latitude = (chi+s)*180./M_PI;
	*longitude = longitude_0+atan2(sinh(eta), cos(zeta))*180./M_PI;
}

/* Encapsulation of UTM projections. */
enum turtle_return project_utm(const struct turtle_map * map,
	double latitude, double longitude, double * x, double * y)
{
	utm_ll_to_xy(latitude, longitude, map->settings.utm.longitude_0,
		map->settings.utm.hemisphere, x, y);
	return TURTLE_RETURN_SUCCESS;
}

/* Encapsulation of inverse UTM projections.  */
enum turtle_return unproject_utm(const struct turtle_map * map,
	double x, double y, double * latitude, double * longitude)
{
	utm_xy_to_ll(x, y, map->settings.utm.longitude_0,
		map->settings.utm.hemisphere, latitude, longitude);
	return TURTLE_RETURN_SUCCESS;
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

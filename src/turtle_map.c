/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <libxml/tree.h>
#include <png.h>

#include "turtle.h"
#include "turtle_map.h"
#include "turtle_projection.h"

/* Generic map allocation and initialisation. */
static struct turtle_map * map_create(int nx, int ny, int bit_depth,
	const struct turtle_projection * projection);

/* Low level data loaders/dumpers. */
static struct turtle_map * map_load_gdem2(const char * datum_path,
	const char * projection, const struct turtle_box * box);
static enum turtle_return map_load_png(const char * path,
	const struct turtle_box * box, struct turtle_map ** map);
static enum turtle_return map_dump_png(const struct turtle_map * map,
	const char * path);

/* Create a handle to a new empty map. */
enum turtle_return turtle_map_create(const char * projection,
	const struct turtle_box * box, int nx, int ny, double zmin,
	double zmax, int bit_depth, struct turtle_map ** map)
{
	*map = NULL;

	/* Check the input arguments. */
	if ((nx <= 0) || (ny <= 0) || (zmin > zmax) || ((bit_depth != 8) &&
		(bit_depth != 16))) return TURTLE_RETURN_DOMAIN_ERROR;

	struct turtle_projection proj;
	enum turtle_return rc = turtle_projection_configure(projection, &proj);
	if (rc != TURTLE_RETURN_SUCCESS) return rc;

	/*  Allocate the memory for the new map and initialise it. */
	*map = map_create(nx, ny, bit_depth, &proj);
	if (*map == NULL) return TURTLE_RETURN_MEMORY_ERROR;

	(*map)->x0 = box->x0-fabs(box->half_x);
	(*map)->y0 = box->y0-fabs(box->half_y);
	(*map)->z0 = zmin;
	(*map)->dx = (nx > 1) ? 2.*fabs(box->half_x)/(nx-1) : 0.;
	(*map)->dy = (ny > 1) ? 2.*fabs(box->half_y)/(ny-1) : 0.;
	(*map)->dz = (zmax-zmin)/(pow(2., bit_depth)-1);
	memset((*map)->z, 0x0, (bit_depth/8)*nx*ny);

	return TURTLE_RETURN_SUCCESS;
}

/* Release the map memory. */
void turtle_map_destroy(struct turtle_map ** map)
{
	if ((map == NULL) || (*map == NULL)) return;
	free(*map);
	*map = NULL;
}

/* Get the file extension in a path. */
static const char * path_extension(const char * path)
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

/* Fill in a map node with an elevation value. */
enum turtle_return turtle_map_fill(struct turtle_map * map, int ix, int iy,
	double elevation)
{
	if (map == NULL)
		return TURTLE_RETURN_MEMORY_ERROR;
	else if ((ix < 0) || (ix >= map->nx) || (iy < 0) || (iy >= map->ny))
		return TURTLE_RETURN_DOMAIN_ERROR;

	if ((map->dz <= 0.) && (elevation != map->z0))
		return TURTLE_RETURN_DOMAIN_ERROR;
	const int iz = (int)((elevation-map->z0)/map->dz+0.5-FLT_EPSILON);
	const int nz = (map->bit_depth == 8) ? 256 : 65536;
	if ((iz < 0) || (iz >= nz))
		return TURTLE_RETURN_DOMAIN_ERROR;

	if (map->bit_depth == 8) {
		unsigned char * z = (unsigned char *)map->z;
		z[iy*map->nx+ix] = (unsigned char)iz;
	}
	else {
		uint16_t * z = (uint16_t *)map->z;
		z[iy*map->nx+ix] = (uint16_t)iz;
	}

	return TURTLE_RETURN_SUCCESS;
}

/* Get the properties of a map node. */
enum turtle_return turtle_map_node(struct turtle_map * map, int ix,
	int iy, double * x, double * y, double * elevation)
{
	if (map == NULL)
		return TURTLE_RETURN_MEMORY_ERROR;
	else if ((ix < 0) || (ix >= map->nx) || (iy < 0) || (iy >= map->ny))
		return TURTLE_RETURN_DOMAIN_ERROR;

	if (x != NULL)
		*x = map->x0+ix*map->dx;
	if (y != NULL)
		*y = map->y0+iy*map->dy;
	if (elevation != NULL) {
		if (map->bit_depth == 8) {
			unsigned char * z = (unsigned char *)map->z;
			*elevation = map->z0+map->dz*z[iy*map->nx+ix];
		}
		else {
			uint16_t * z = (uint16_t *)map->z;
			*elevation = map->z0+map->dz*z[iy*map->nx+ix];
		}
	}

	return TURTLE_RETURN_SUCCESS;
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

const struct turtle_projection * turtle_map_projection(
	const struct turtle_map * map)
{
	if (map == NULL) return NULL;
	return &map->projection;
}

/* Get some basic information on a map. */
enum turtle_return turtle_map_info(const struct turtle_map * map,
	struct turtle_box * box, double * zmin, double * zmax)
{
	if (box != NULL) {
		box->half_x = 0.5*(map->nx-1)*map->dx;
		box->half_y = 0.5*(map->ny-1)*map->dy;
		box->x0 = map->x0+box->half_x;
		box->y0 = map->y0+box->half_y;
	}
	if (zmin != NULL) *zmin = map->z0;
	if (zmax != NULL) *zmax = map->z0+(pow(2, map->bit_depth)-1)*map->dz;

	return TURTLE_RETURN_SUCCESS;
}

static struct turtle_map * map_create(int nx, int ny, int bit_depth,
	const struct turtle_projection * projection)
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
	memcpy(&map->projection, projection, sizeof(*projection));
	char * p = map->data;
	map->z = (void *)(p);

	return map;
}

static const char * get_node_content(xmlNode * node)
{
	xmlNode * subnode;
	for (subnode = node->children; subnode != NULL;
		subnode=subnode->next) if (subnode->type == XML_TEXT_NODE) {
		return (const char *)subnode->content;
	}
	return NULL;
}

static enum turtle_return map_load_png(const char* path,
	const struct turtle_box * box, struct turtle_map ** map)
{
	FILE * fid = NULL;
	png_bytep * row_pointers = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	int nx = 0, ny = 0;
	struct turtle_projection projection;
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
					if ((rc = turtle_projection_configure(
						content, &projection)) !=
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
	*map = map_create(dnx, dny, bit_depth, &projection);
	if (*map == NULL) {
		rc = TURTLE_RETURN_MEMORY_ERROR;
		goto error;
	}
	(*map)->x0 = x0+nx0*dx;
	(*map)->y0 = y0+ny0*dy;
	(*map)->z0 = z0;
	(*map)->dx = dx;
	(*map)->dy = dy;
	(*map)->dz = dz;

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

	rc = TURTLE_RETURN_SUCCESS;
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

	return rc;
}

/* Dump a map in png format. */
static enum turtle_return map_dump_png(const struct turtle_map * map,
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
	const char * tmp;
	char * projection_tag;
	const struct turtle_projection * projection = turtle_map_projection(map);
	turtle_projection_info(projection, &projection_tag);
	if (projection_tag == NULL)
		tmp = "";
	else
		tmp = projection_tag;
	char header[2048];
	sprintf(header, "<topography><x0>%.3lf</x0><y0>%.3lf</y0>"
		"<z0>%.3lf</z0><dx>%.3lf</dx><dy>%.3lf</dy>"
		"<dz>%.5e</dz><projection>%s</projection></topography>",
		map->x0, map->y0, map->z0, map->dx, map->dy, map->dz,
		tmp);
	free(projection_tag);
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

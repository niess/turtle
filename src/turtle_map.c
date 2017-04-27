/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 * 
 * Topographic Utilities for Rendering The eLEvation (TURTLE)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/*
 * Turtle projection map handle for managing local maps.
 */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef TURTLE_NO_PNG
/* Required for .png map format. */
#include "jsmn.h"
#include <arpa/inet.h>
#include <png.h>
#endif

#include "turtle.h"
#include "turtle_map.h"
#include "turtle_projection.h"
#include "turtle_return.h"

/* Generic map allocation and initialisation. */
static struct turtle_map * map_create(
    int nx, int ny, int bit_depth, const struct turtle_projection * projection);

/* Low level data loaders/dumpers. */
#ifndef TURTLE_NO_PNG
static enum turtle_return map_load_png(
    const char * path, const struct turtle_box * box, struct turtle_map ** map);
static enum turtle_return map_dump_png(
    const struct turtle_map * map, const char * path);
#endif

/* Create a handle to a new empty map. */
enum turtle_return turtle_map_create(const char * projection,
    const struct turtle_box * box, int nx, int ny, double zmin, double zmax,
    int bit_depth, struct turtle_map ** map)
{
        *map = NULL;

        /* Check the input arguments. */
        if ((nx <= 0) || (ny <= 0) || (zmin > zmax) ||
            ((bit_depth != 8) && (bit_depth != 16)))
                TURTLE_RETURN(TURTLE_RETURN_DOMAIN_ERROR, turtle_map_create);

        struct turtle_projection proj;
        enum turtle_return rc = turtle_projection_configure(projection, &proj);
        if (rc != TURTLE_RETURN_SUCCESS) TURTLE_RETURN(rc, turtle_map_create);

        /*  Allocate the memory for the new map and initialise it. */
        *map = map_create(nx, ny, bit_depth, &proj);
        if (*map == NULL)
                TURTLE_RETURN(TURTLE_RETURN_MEMORY_ERROR, turtle_map_create);

        (*map)->x0 = box->x0 - fabs(box->half_x);
        (*map)->y0 = box->y0 - fabs(box->half_y);
        (*map)->z0 = zmin;
        (*map)->dx = (nx > 1) ? 2. * fabs(box->half_x) / (nx - 1) : 0.;
        (*map)->dy = (ny > 1) ? 2. * fabs(box->half_y) / (ny - 1) : 0.;
        (*map)->dz = (zmax - zmin) / (pow(2., bit_depth) - 1);
        memset((*map)->z, 0x0, (bit_depth / 8) * nx * ny);

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
enum turtle_return turtle_map_load(
    const char * path, const struct turtle_box * box, struct turtle_map ** map)
{
        /* Check the file extension. */
        const char * ext = path_extension(path);
        if (ext == NULL)
                TURTLE_RETURN(TURTLE_RETURN_BAD_EXTENSION, turtle_map_load);

#ifndef TURTLE_NO_PNG
        if (strcmp(ext, "png") == 0)
                TURTLE_RETURN(map_load_png(path, box, map), turtle_map_load);
        else
                TURTLE_RETURN(TURTLE_RETURN_BAD_EXTENSION, turtle_map_load);
#else
        TURTLE_RETURN(TURTLE_RETURN_BAD_EXTENSION, turtle_map_load);
#endif
}

/* Save the map to disk. */
enum turtle_return turtle_map_dump(
    const struct turtle_map * map, const char * path)
{
        /* Check the file extension. */
        const char * ext = path_extension(path);
        if (ext == NULL)
                TURTLE_RETURN(TURTLE_RETURN_BAD_EXTENSION, turtle_map_dump);

#ifndef TURTLE_NO_PNG
        if (strcmp(ext, "png") == 0)
                TURTLE_RETURN(map_dump_png(map, path), turtle_map_dump);
        else
                TURTLE_RETURN(TURTLE_RETURN_BAD_EXTENSION, turtle_map_dump);
#else
        TURTLE_RETURN(TURTLE_RETURN_BAD_EXTENSION, turtle_map_dump);
#endif
}

/* Fill in a map node with an elevation value. */
enum turtle_return turtle_map_fill(
    struct turtle_map * map, int ix, int iy, double elevation)
{
        if (map == NULL)
                TURTLE_RETURN(TURTLE_RETURN_MEMORY_ERROR, turtle_map_fill);
        else if ((ix < 0) || (ix >= map->nx) || (iy < 0) || (iy >= map->ny))
                TURTLE_RETURN(TURTLE_RETURN_DOMAIN_ERROR, turtle_map_fill);

        if ((map->dz <= 0.) && (elevation != map->z0))
                TURTLE_RETURN(TURTLE_RETURN_DOMAIN_ERROR, turtle_map_fill);
        const int iz =
            (int)((elevation - map->z0) / map->dz + 0.5 - FLT_EPSILON);
        const int nz = (map->bit_depth == 8) ? 256 : 65536;
        if ((iz < 0) || (iz >= nz))
                TURTLE_RETURN(TURTLE_RETURN_DOMAIN_ERROR, turtle_map_fill);

        if (map->bit_depth == 8) {
                unsigned char * z = (unsigned char *)map->z;
                z[iy * map->nx + ix] = (unsigned char)iz;
        } else {
                uint16_t * z = (uint16_t *)map->z;
                z[iy * map->nx + ix] = (uint16_t)iz;
        }

        return TURTLE_RETURN_SUCCESS;
}

/* Get the properties of a map node. */
enum turtle_return turtle_map_node(struct turtle_map * map, int ix, int iy,
    double * x, double * y, double * elevation)
{
        if (map == NULL)
                TURTLE_RETURN(TURTLE_RETURN_MEMORY_ERROR, turtle_map_node);
        else if ((ix < 0) || (ix >= map->nx) || (iy < 0) || (iy >= map->ny))
                TURTLE_RETURN(TURTLE_RETURN_DOMAIN_ERROR, turtle_map_node);

        if (x != NULL) *x = map->x0 + ix * map->dx;
        if (y != NULL) *y = map->y0 + iy * map->dy;
        if (elevation != NULL) {
                if (map->bit_depth == 8) {
                        unsigned char * z = (unsigned char *)map->z;
                        *elevation = map->z0 + map->dz * z[iy * map->nx + ix];
                } else {
                        uint16_t * z = (uint16_t *)map->z;
                        *elevation = map->z0 + map->dz * z[iy * map->nx + ix];
                }
        }

        return TURTLE_RETURN_SUCCESS;
}

/* Interpolate the elevation at a given location. */
enum turtle_return turtle_map_elevation(
    const struct turtle_map * map, double x, double y, double * z)
{
        double hx = (x - map->x0) / map->dx;
        double hy = (y - map->y0) / map->dy;
        int ix = (int)hx;
        int iy = (int)hy;

        if (ix >= map->nx - 1 || hx < 0 || iy >= map->ny - 1 || hy < 0)
                TURTLE_RETURN(TURTLE_RETURN_DOMAIN_ERROR, turtle_map_elevation);
        hx -= ix;
        hy -= iy;

        const int nx = map->nx;
        if (map->bit_depth == 8) {
                const unsigned char * zm = map->z;
                *z = zm[iy * nx + ix] * (1. - hx) * (1. - hy) +
                    zm[(iy + 1) * nx + ix] * (1. - hx) * hy +
                    zm[iy * nx + ix + 1] * hx * (1. - hy) +
                    zm[(iy + 1) * nx + ix + 1] * hx * hy;
        } else {
                const uint16_t * zm = map->z;
                *z = zm[iy * nx + ix] * (1. - hx) * (1. - hy) +
                    zm[(iy + 1) * nx + ix] * (1. - hx) * hy +
                    zm[iy * nx + ix + 1] * hx * (1. - hy) +
                    zm[(iy + 1) * nx + ix + 1] * hx * hy;
        }
        *z = (*z) * map->dz + map->z0;
        return TURTLE_RETURN_SUCCESS;
}

struct turtle_projection * turtle_map_projection(struct turtle_map * map)
{
        if (map == NULL) return NULL;
        return &map->projection;
}

/* Get some basic information on a map. */
void turtle_map_info(const struct turtle_map * map, struct turtle_box * box,
    int * nx, int * ny, double * zmin, double * zmax, int * bit_depth)
{
        if (box != NULL) {
                box->half_x = 0.5 * (map->nx - 1) * map->dx;
                box->half_y = 0.5 * (map->ny - 1) * map->dy;
                box->x0 = map->x0 + box->half_x;
                box->y0 = map->y0 + box->half_y;
        }
        if (nx != NULL) *nx = map->nx;
        if (ny != NULL) *ny = map->ny;
        if (zmin != NULL) *zmin = map->z0;
        if (zmax != NULL)
                *zmax = map->z0 + (pow(2, map->bit_depth) - 1) * map->dz;
        if (bit_depth != NULL) *bit_depth = map->bit_depth;
}

static struct turtle_map * map_create(
    int nx, int ny, int bit_depth, const struct turtle_projection * projection)
{
        /* Allocate the map memory. */
        int datum_size;
        if (bit_depth == 8)
                datum_size = sizeof(unsigned char);
        else
                datum_size = sizeof(uint16_t);
        struct turtle_map * map = malloc(sizeof(*map) + nx * ny * datum_size);
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

#ifndef TURTLE_NO_PNG
static inline int json_strcmp(
    const char * json, jsmntok_t * token, const char * string)
{
        return strncmp(json + token->start, string, token->end - token->start);
}

static inline int json_sscanf(
    const char * json, jsmntok_t * token, double * value)
{
        return sscanf(json + token->start, "%lf", value);
}

static enum turtle_return map_load_png(
    const char * path, const struct turtle_box * box, struct turtle_map ** map)
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
                rc = TURTLE_RETURN_PATH_ERROR;
                goto error;
        }

        char header[8];
        rc = TURTLE_RETURN_BAD_FORMAT;
        if (fread(header, 1, 8, fid) != 8) goto error;
        if (png_sig_cmp((png_bytep)header, 0, 8) != 0) goto error;

        /* initialize libpng containers. */
        png_ptr =
            png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
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

        /* Parse the JSON meta data. */
        rc = TURTLE_RETURN_BAD_JSON;
        double x0 = 0., y0 = 0., z0 = 0., dx = 1., dy = 1., dz = 1.;
        png_textp text_ptr;
        int num_text;
        unsigned n_chunks =
            png_get_text(png_ptr, info_ptr, &text_ptr, &num_text);
        if (n_chunks > 0) {
                int i = 0;
                png_text * p = text_ptr;
                for (; i < num_text; i++) {
                        char * text = p->text;
                        unsigned int length = p->text_length;
                        p++;

                        /* Map all the tokens with JSMN. */
                        jsmn_parser parser;
#define N_TOKENS 17
                        jsmntok_t tokens[N_TOKENS];
                        jsmn_init(&parser);
                        const int r =
                            jsmn_parse(&parser, text, length, tokens, N_TOKENS);
                        if (r < N_TOKENS) continue;

                        /* Check for a "topography" object. */
                        if ((tokens[0].type != JSMN_OBJECT) ||
                            (tokens[1].type != JSMN_STRING))
                                continue;
                        if (json_strcmp(text, tokens + 1, "topography") != 0)
                                continue;
#define N_FIELDS 7
                        if ((tokens[2].type != JSMN_OBJECT) ||
                            (tokens[2].size != N_FIELDS))
                                goto error;

                        /* Parse the "topography" fields. */
                        int j, done[N_FIELDS];
                        jsmntok_t *key, *value;
                        memset(done, 0x0, sizeof(done));
                        for (j = 0, key = tokens + 3, value = tokens + 4;
                             j < N_FIELDS; j++, key += 2, value += 2) {
                                if (key->type != JSMN_STRING) goto error;
                                if (!done[0] && (json_strcmp(text, key,
                                                     "projection") == 0)) {
                                        if (value->type != JSMN_STRING)
                                                goto error;
                                        text[value->end] = '\0';
                                        if ((rc = turtle_projection_configure(
                                                 text + value->start,
                                                 &projection)) !=
                                            TURTLE_RETURN_SUCCESS)
                                                goto error;
                                        rc = TURTLE_RETURN_BAD_JSON;
                                        done[0] = 1;
                                } else {
                                        if (value->type != JSMN_PRIMITIVE)
                                                goto error;

                                        if (!done[1] && (json_strcmp(text, key,
                                                             "x0") == 0)) {
                                                if (json_sscanf(
                                                        text, value, &x0) != 1)
                                                        goto error;
                                                done[1] = 1;
                                        } else if (!done[2] &&
                                            (json_strcmp(text, key, "y0") ==
                                                0)) {
                                                if (json_sscanf(
                                                        text, value, &y0) != 1)
                                                        goto error;
                                                done[2] = 1;
                                        } else if (!done[3] &&
                                            (json_strcmp(text, key, "z0") ==
                                                0)) {
                                                if (json_sscanf(
                                                        text, value, &z0) != 1)
                                                        goto error;
                                                done[3] = 1;
                                        } else if (!done[4] &&
                                            (json_strcmp(text, key, "dx") ==
                                                0)) {
                                                if (json_sscanf(
                                                        text, value, &dx) != 1)
                                                        goto error;
                                                done[4] = 1;
                                        } else if (!done[5] &&
                                            (json_strcmp(text, key, "dy") ==
                                                0)) {
                                                if (json_sscanf(
                                                        text, value, &dy) != 1)
                                                        goto error;
                                                done[5] = 1;
                                        } else if (!done[6] &&
                                            (json_strcmp(text, key, "dz") ==
                                                0)) {
                                                if (json_sscanf(
                                                        text, value, &dz) != 1)
                                                        goto error;
                                                done[6] = 1;
                                        } else
                                                goto error;
                                }
                        }
#undef N_FIELDS
#undef N_TOKENS
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
        } else {
                /* Clip the bounding box to the full map. */
                const double xlow = box->x0 - box->half_x;
                const double xhigh = box->x0 + box->half_x;
                const double ylow = box->y0 - box->half_y;
                const double yhigh = box->y0 + box->half_y;

                rc = TURTLE_RETURN_DOMAIN_ERROR;
                if (xlow <= x0)
                        nx0 = 0;
                else
                        nx0 = (int)((xlow - x0) / dx);
                if (nx0 >= nx) goto error;
                dnx = (int)((xhigh - x0) / dx) + 1;
                if (dnx > nx)
                        dnx = nx;
                else if (dnx <= nx0)
                        goto error;
                dnx -= nx0;

                if (ylow <= y0)
                        ny0 = 0;
                else
                        ny0 = (int)((ylow - y0) / dy);
                if (ny0 >= ny) goto error;
                dny = (int)((yhigh - y0) / dy) + 1;
                if (dny > ny)
                        dny = ny;
                else if (dny <= ny0)
                        goto error;
                dny -= ny0;
        }

        /* Allocate the map and copy the data. */
        *map = map_create(dnx, dny, bit_depth, &projection);
        if (*map == NULL) {
                rc = TURTLE_RETURN_MEMORY_ERROR;
                goto error;
        }
        (*map)->x0 = x0 + nx0 * dx;
        (*map)->y0 = y0 + ny0 * dy;
        (*map)->z0 = z0;
        (*map)->dx = dx;
        (*map)->dy = dy;
        (*map)->dz = dz;

        unsigned char * z8 = (*map)->z;
        uint16_t * z16 = (*map)->z;
        for (i = ny - ny0 - 1; i >= ny - ny0 - dny; i--) {
                png_bytep ptr = row_pointers[i] + nx0 * (bit_depth / 8);
                int j = 0;
                for (; j < dnx; j++) {
                        if (bit_depth == 8) {
                                *z8 = (unsigned char)*ptr;
                                z8++;
                                ptr += 1;
                        } else {
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
static enum turtle_return map_dump_png(
    const struct turtle_map * map, const char * path)
{
        FILE * fid = NULL;
        png_bytep * row_pointers = NULL;
        png_structp png_ptr = NULL;
        png_infop info_ptr = NULL;
        int header_size = 2048;
        char * header = NULL;
        enum turtle_return rc;

        /* Check the bit depth. */
        rc = TURTLE_RETURN_BAD_FORMAT;
        if ((map->bit_depth != 8) && (map->bit_depth != 16)) goto exit;

        /* Initialise the file and the PNG pointers. */
        rc = TURTLE_RETURN_PATH_ERROR;
        fid = fopen(path, "wb+");
        if (fid == NULL) goto exit;

        rc = TURTLE_RETURN_MEMORY_ERROR;
        png_ptr =
            png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png_ptr == NULL) goto exit;
        info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == NULL) goto exit;
        if (setjmp(png_jmpbuf(png_ptr))) goto exit;
        png_init_io(png_ptr, fid);

        /* Write the header. */
        const int nx = map->nx, ny = map->ny;
        png_set_IHDR(png_ptr, info_ptr, nx, ny, map->bit_depth,
            PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);
        const char * tmp;
        char * projection_tag;
        const struct turtle_projection * projection = &map->projection;
        turtle_projection_info(projection, &projection_tag);
        if (projection_tag == NULL)
                tmp = "";
        else
                tmp = projection_tag;
        for (;;) {
                char * new = realloc(header, header_size);
                if (new == NULL) goto exit;
                header = new;
                if (snprintf(header, header_size,
                        "{\"topography\" : {\"x0\" : %.3lf, \"y0\" : %.3lf, "
                        "\"z0\" : %.3lf, \"dx\" : %.3lf, \"dy\" : %.3lf, "
                        "\"dz\" : %.5e, \"projection\" : \"%s\"}}",
                        map->x0, map->y0, map->z0, map->dx, map->dy, map->dz,
                        tmp) < header_size)
                        break;
                header_size += 2048;
        }
        free(projection_tag);
        png_text text[] = { { PNG_TEXT_COMPRESSION_NONE, "Comment", header,
            strlen(header) } };
        png_set_text(png_ptr, info_ptr, text, sizeof(text) / sizeof(text[0]));
        png_write_info(png_ptr, info_ptr);
        free(header);
        header = NULL;

        /* Write the data */
        row_pointers = (png_bytep *)calloc(ny, sizeof(png_bytep));
        if (row_pointers == NULL) goto exit;
        int i = 0;
        for (; i < ny; i++) {
                row_pointers[i] = (png_byte *)malloc(
                    (map->bit_depth / 8) * nx * sizeof(char));
                if (row_pointers[i] == NULL) goto exit;
                if (map->bit_depth == 8) {
                        unsigned char * ptr = (unsigned char *)row_pointers[i];
                        const unsigned char * z =
                            (const unsigned char *)(map->z) + (ny - i - 1) * nx;
                        int j = 0;
                        for (; j < nx; j++) {
                                *ptr = *z;
                                z++;
                                ptr++;
                        }
                } else {
                        uint16_t * ptr = (uint16_t *)row_pointers[i];
                        const uint16_t * z =
                            (const uint16_t *)(map->z) + (ny - i - 1) * nx;
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
        free(header);
        if (fid != NULL) fclose(fid);
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
#endif

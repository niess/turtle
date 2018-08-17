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

/* C89 standard library */
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef TURTLE_NO_PNG
/* For byte order */
#include <arpa/inet.h>
/* PNG library */
#include <png.h>
#endif
/* TURTLE library */
#include "turtle.h"
#include "turtle/error.h"
#include "turtle/map.h"
#include "turtle/projection.h"
#include "turtle/reader.h"
#include "turtle/stack.h"

/* Low level data loaders/dumpers. */
#ifndef TURTLE_NO_PNG
static enum turtle_return map_dump_png(
    const struct turtle_map * map, const char * path);
#endif

/* Default data getter */
static double get_default_z(const struct turtle_map * map, int ix, int iy)
{
        return map->meta.z0 + map->data[iy * map->meta.nx + ix] * map->meta.dz;
}

/* Default data setter */
static void set_default_z(struct turtle_map * map, int ix, int iy, double z)
{
        const double d = round((z - map->meta.z0) / map->meta.dz);
        map->data[iy * map->meta.nx + ix] = (uint16_t)d;
}

/* Create a handle to a new empty map */
enum turtle_return turtle_map_create(struct turtle_map ** map,
    const struct turtle_map_info * info, const char * projection)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_create);
        *map = NULL;

        /* Check the input arguments. */
        if ((info->nx <= 0) || (info->ny <= 0) || (info->z[0] == info->z[1]))
                return TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_DOMAIN_ERROR, "invalid input parameter(s)");

        struct turtle_projection proj;
        if (turtle_projection_configure_(projection, &proj, error_) !=
            TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();

        /* Allocate the map memory */
        *map =
            malloc(sizeof(**map) + info->nx * info->ny * sizeof(*(*map)->data));
        if (*map == NULL) return TURTLE_ERROR_MEMORY();

        /* Fill the identifiers */
        (*map)->meta.nx = info->nx;
        (*map)->meta.ny = info->ny;
        (*map)->meta.x0 = info->x[0];
        (*map)->meta.y0 = info->y[0];
        (*map)->meta.z0 = info->z[0];
        (*map)->meta.dx =
            (info->nx > 1) ? (info->x[1] - info->x[0]) / (info->nx - 1) : 0.;
        (*map)->meta.dy =
            (info->ny > 1) ? (info->y[1] - info->y[0]) / (info->ny - 1) : 0.;
        (*map)->meta.dz = (info->z[1] - info->z[0]) / 65535;
        memcpy(
            &(*map)->meta.projection, &proj, sizeof((*map)->meta.projection));
        memset((*map)->data, 0x0, sizeof(*(*map)->data) * info->nx * info->ny);

        (*map)->meta.get_z = &get_default_z;
        (*map)->meta.set_z = &set_default_z;
        strcpy((*map)->meta.encoding, "none");

        (*map)->stack = NULL;
        (*map)->prev = (*map)->next = NULL;
        (*map)->clients = 0;

        return TURTLE_RETURN_SUCCESS;
}

/* Release the map memory and update any stack */
void turtle_map_destroy(struct turtle_map ** map)
{
        if ((map == NULL) || (*map == NULL)) return;

        if ((*map)->stack != NULL) {
                /* Update the stack */
                struct turtle_map * prev = (*map)->prev;
                struct turtle_map * next = (*map)->next;
                if (prev != NULL) prev->next = next;
                if (next != NULL)
                        next->prev = prev;
                else
                        (*map)->stack->head = prev;
                (*map)->stack->size--;
        }

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
enum turtle_return turtle_map_load(const char * path, struct turtle_map ** map)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_load);

        /* Get a reader for the file */
        struct turtle_reader * reader;
        if (turtle_reader_create_(&reader, path, error_) !=
            TURTLE_RETURN_SUCCESS)
                goto exit;

        /* Load the meta data */
        if (reader->open(reader, path, error_) != TURTLE_RETURN_SUCCESS)
                goto exit;

        /* Allocate the map */
        *map = malloc(sizeof(**map) +
            reader->meta.nx * reader->meta.ny * sizeof(*(*map)->data));
        if (*map == NULL) {
                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for map `%s'", path);
                goto exit;
        }

        /* Initialise the map data */
        memcpy(&(*map)->meta, &reader->meta, sizeof((*map)->meta));
        (*map)->stack = NULL;
        (*map)->prev = (*map)->next = NULL;
        (*map)->clients = 0;

        /* Load the topography data */
        if (reader->read(reader, *map, error_) != TURTLE_RETURN_SUCCESS) {
                free(*map);
                *map = NULL;
                goto exit;
        }

        /* Finalise the reader */
        reader->close(reader);
exit:
        free(reader);
        return TURTLE_ERROR_RAISE();
}

/* Save the map to disk */
enum turtle_return turtle_map_dump(
    const struct turtle_map * map, const char * path)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_dump);

        /* Check the file extension. */
        const char * ext = path_extension(path);
        if (ext == NULL) return TURTLE_ERROR_NO_EXTENSION();

#ifndef TURTLE_NO_PNG
        if (strcmp(ext, "png") == 0) {
                enum turtle_return rc = map_dump_png(map, path);
                if (rc == TURTLE_RETURN_SUCCESS) {
                        return TURTLE_RETURN_SUCCESS;
                } else if (rc == TURTLE_RETURN_PATH_ERROR) {
                        return TURTLE_ERROR_PATH(path);
                } else if (rc == TURTLE_RETURN_BAD_FORMAT) {
                        return TURTLE_ERROR_MESSAGE(
                            TURTLE_RETURN_BAD_FORMAT, "invalid data format");
                } else if (rc == TURTLE_RETURN_MEMORY_ERROR) {
                        return TURTLE_ERROR_MEMORY();
                } else {
                        return TURTLE_ERROR_UNEXPECTED(rc);
                }
        } else {
                return TURTLE_ERROR_EXTENSION(ext);
        }
#else
        return TURTLE_ERROR_EXTENSION(ext);
#endif
}

/* Fill in a map node with an elevation value */
enum turtle_return turtle_map_fill(
    struct turtle_map * map, int ix, int iy, double elevation)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_fill);

        if (map == NULL) {
                return TURTLE_ERROR_MEMORY();
        } else if ((ix < 0) || (ix >= map->meta.nx) || (iy < 0) ||
            (iy >= map->meta.ny)) {
                return TURTLE_ERROR_OUTSIDE_MAP();
        }

        if ((map->meta.dz <= 0.) && (elevation != map->meta.z0))
                return TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_DOMAIN_ERROR, "inconsistent elevation value");
        if ((elevation < map->meta.z0) ||
            (elevation >= map->meta.z0 + 65535 * map->meta.dz))
                return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_DOMAIN_ERROR,
                    "elevation is outside of map span");
        map->meta.set_z(map, ix, iy, elevation);

        return TURTLE_RETURN_SUCCESS;
}

/* Get the properties of a map node */
enum turtle_return turtle_map_node(struct turtle_map * map, int ix, int iy,
    double * x, double * y, double * elevation)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_node);

        if (map == NULL) {
                return TURTLE_ERROR_MEMORY();
        } else if ((ix < 0) || (ix >= map->meta.nx) || (iy < 0) ||
            (iy >= map->meta.ny)) {
                return TURTLE_ERROR_OUTSIDE_MAP();
        }

        if (x != NULL) *x = map->meta.x0 + ix * map->meta.dx;
        if (y != NULL) *y = map->meta.y0 + iy * map->meta.dy;
        if (elevation != NULL) {
                *elevation =
                    map->meta.z0 + map->meta.dz * map->meta.get_z(map, ix, iy);
        }

        return TURTLE_RETURN_SUCCESS;
}

/* Interpolate the elevation at a given location */
enum turtle_return turtle_map_elevation_(const struct turtle_map * map,
    double x, double y, double * z, int * inside,
    struct turtle_error_context * error_)
{
        double hx = (x - map->meta.x0) / map->meta.dx;
        double hy = (y - map->meta.y0) / map->meta.dy;
        int ix = (int)hx;
        int iy = (int)hy;

        if (ix >= map->meta.nx - 1 || hx < 0 || iy >= map->meta.ny - 1 ||
            hy < 0) {
                if (inside != NULL) {
                        *inside = 0;
                        return TURTLE_RETURN_SUCCESS;
                } else {
                        return TURTLE_ERROR_OUTSIDE_MAP();
                }
        }
        hx -= ix;
        hy -= iy;

        turtle_map_getter_t * get_z = map->meta.get_z;
        const double z00 = get_z(map, ix, iy);
        const double z10 = get_z(map, ix + 1, iy);
        const double z01 = get_z(map, ix, iy + 1);
        const double z11 = get_z(map, ix + 1, iy + 1);
        *z = z00 * (1. - hx) * (1. - hy) + z01 * (1. - hx) * hy +
            z10 * hx * (1. - hy) + z11 * hx * hy;

        if (inside != NULL) *inside = 1;
        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_map_elevation(
    const struct turtle_map * map, double x, double y, double * z, int * inside)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_elevation);
        return turtle_map_elevation_(map, x, y, z, inside, error_);
}

struct turtle_projection * turtle_map_projection(struct turtle_map * map)
{
        if (map == NULL) return NULL;
        return &map->meta.projection;
}

/* Get the map meta data */
void turtle_map_meta(const struct turtle_map * map,
    struct turtle_map_info * info, const char ** projection)
{
        if (info != NULL) {
                info->nx = map->meta.nx;
                info->ny = map->meta.ny;
                info->x[0] = map->meta.x0;
                info->x[1] = map->meta.x0 + (map->meta.nx - 1) * map->meta.dx;
                info->y[0] = map->meta.y0;
                info->y[1] = map->meta.y0 + (map->meta.ny - 1) * map->meta.dy;
                info->z[0] = map->meta.z0;
                info->z[1] = map->meta.z0 + 65535 * map->meta.dz;
                info->encoding = map->meta.encoding;
        }

        if (projection != NULL) {
                *projection = turtle_projection_name(&map->meta.projection);
        }
}

#ifndef TURTLE_NO_PNG

/* Dump a map in png format */
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

        /* Initialise the file and the PNG pointers */
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
        const int nx = map->meta.nx, ny = map->meta.ny;
        png_set_IHDR(png_ptr, info_ptr, nx, ny, 16, PNG_COLOR_TYPE_GRAY,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);
        const char * tmp;
        const struct turtle_projection * projection = &map->meta.projection;
        const char * projection_tag = turtle_projection_name(projection);
        if (projection_tag == NULL)
                tmp = "";
        else
                tmp = projection_tag;
        for (;;) {
                char * new = realloc(header, header_size);
                if (new == NULL) goto exit;
                header = new;
                const double x1 = map->meta.x0 + map->meta.dx * map->meta.nx;
                const double y1 = map->meta.y0 + map->meta.dy * map->meta.ny;
                const double z1 = map->meta.z0 + map->meta.dz * 65535;
                if (snprintf(header, header_size,
                        "{\"topography\" : {\"x0\" : %.5lf, \"y0\" : %.5lf, "
                        "\"z0\" : %.5lf, \"x1\" : %.5lf, \"y1\" : %.5lf, "
                        "\"z1\" : %.5lf, \"projection\" : \"%s\"}}",
                        map->meta.x0, map->meta.y0, map->meta.z0, x1, y1, z1,
                        tmp) < header_size)
                        break;
                header_size += 2048;
        }
        png_text text[] = { { PNG_TEXT_COMPRESSION_NONE, "Comment", header,
            strlen(header) } };
        png_set_text(png_ptr, info_ptr, text, sizeof(text) / sizeof(text[0]));
        png_write_info(png_ptr, info_ptr);
        free(header);
        header = NULL;

        /* Write the data */
        row_pointers = (png_bytep *)calloc(ny, sizeof(png_bytep));
        if (row_pointers == NULL) goto exit;
        int i;
        for (i = 0; i < ny; i++) {
                row_pointers[i] = (png_byte *)malloc(nx * sizeof(uint16_t));
                if (row_pointers[i] == NULL) goto exit;
                uint16_t * ptr = (uint16_t *)row_pointers[i];
                int j;
                for (j = 0; j < nx; j++) {
                        const double d =
                            round((map->meta.get_z(map, j, ny - 1 - i) -
                                      map->meta.z0) /
                                map->meta.dz);
                        *ptr = (uint16_t)htons(d);
                        ptr++;
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

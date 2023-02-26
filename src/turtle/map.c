/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicules over Long rangEs (TURTLE)
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
/* TURTLE library */
#include "turtle.h"
#include "turtle/error.h"
#include "turtle/io.h"
#include "turtle/list.h"
#include "turtle/map.h"
#include "turtle/projection.h"
#include "turtle/stack.h"

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
        if (turtle_projection_configure_(&proj, projection, error_) !=
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
        memset(&(*map)->element, 0x0, sizeof((*map)->element));
        (*map)->clients = 0;

        return TURTLE_RETURN_SUCCESS;
}

/* Release the map memory and update any stack */
void turtle_map_destroy(struct turtle_map ** map)
{
        if ((map == NULL) || (*map == NULL)) return;

        if ((*map)->stack != NULL) {
                /* Update the stack */
                turtle_list_remove_(&(*map)->stack->tiles, *map);
        }

        free(*map);
        *map = NULL;
}

/* Load a map from a data file */
enum turtle_return turtle_map_load_(struct turtle_map ** map, const char * path,
    struct turtle_error_context * error_)
{
        /* Get an io manager for the file */
        struct turtle_io * io;
        if (turtle_io_create_(&io, path, error_) != TURTLE_RETURN_SUCCESS)
                goto exit;

        /* Load the meta data */
        if (io->open(io, path, "rb", error_) != TURTLE_RETURN_SUCCESS)
                goto exit;

        /* Allocate the map */
        *map = malloc(
            sizeof(**map) + io->meta.nx * io->meta.ny * sizeof(*(*map)->data));
        if (*map == NULL) {
                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for map `%s'", path);
                goto exit;
        }

        /* Initialise the map data */
        memcpy(&(*map)->meta, &io->meta, sizeof((*map)->meta));
        (*map)->stack = NULL;
        memset(&(*map)->element, 0x0, sizeof((*map)->element));
        (*map)->clients = 0;

        /* Load the topography data */
        if (io->read(io, *map, error_) != TURTLE_RETURN_SUCCESS) {
                free(*map);
                *map = NULL;
                goto exit;
        }

        /* Finalise the io manager */
        io->close(io);
exit:
        free(io);
        return error_->code;
}

enum turtle_return turtle_map_load(struct turtle_map ** map, const char * path)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_load);
        turtle_map_load_(map, path, error_);
        return TURTLE_ERROR_RAISE();
}

/* Save the map to disk */
enum turtle_return turtle_map_dump(
    const struct turtle_map * map, const char * path)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_dump);
        struct turtle_io * io;
        if (turtle_io_create_(&io, path, error_) != TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();
        if (io->open(io, path, "wb+", error_) != TURTLE_RETURN_SUCCESS) {
                free(io);
                return TURTLE_ERROR_RAISE();
        }
        io->write(io, map, error_);
        io->close(io);
        free(io);
        return TURTLE_ERROR_RAISE();
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
            (elevation > map->meta.z0 + 65535 * map->meta.dz))
                return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_DOMAIN_ERROR,
                    "elevation is outside of map span");
        map->meta.set_z(map, ix, iy, elevation);

        return TURTLE_RETURN_SUCCESS;
}

/* Get the properties of a map node */
enum turtle_return turtle_map_node(const struct turtle_map * map, int ix,
    int iy, double * x, double * y, double * elevation)
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
        if (elevation != NULL)
                *elevation = map->meta.get_z(map, ix, iy);

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

        if ((hx > map->meta.nx - 1) || (hx < 0) ||
            (hy > map->meta.ny - 1) || (hy < 0)) {
                if (inside != NULL) {
                        *inside = 0;
                        return TURTLE_RETURN_SUCCESS;
                } else {
                        return TURTLE_ERROR_OUTSIDE_MAP();
                }
        }
        if (ix == map->meta.nx - 1) {
                ix--;
                hx = 1.;
        } else
                hx -= ix;
        if (iy == map->meta.ny - 1) {
                iy--;
                hy = 1.;
        } else
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

/* Compute the gradient at a given location */
enum turtle_return turtle_map_gradient_(const struct turtle_map * map,
    double x, double y, double * gx, double * gy, int * inside,
    struct turtle_error_context * error_)
{
        double hx = (x - map->meta.x0) / map->meta.dx;
        double hy = (y - map->meta.y0) / map->meta.dy;
        int ix = (int)hx;
        int iy = (int)hy;

        if ((hx > map->meta.nx - 1) || (hx < 0) ||
            (hy > map->meta.ny - 1) || (hy < 0)) {
                if (inside != NULL) {
                        *inside = 0;
                        return TURTLE_RETURN_SUCCESS;
                } else {
                        return TURTLE_ERROR_OUTSIDE_MAP();
                }
        }
        if (ix == map->meta.nx - 1) {
                ix--;
                hx = 1.;
        } else
                hx -= ix;
        if (iy == map->meta.ny - 1) {
                iy--;
                hy = 1.;
        } else
                hy -= iy;

        turtle_map_getter_t * get_z = map->meta.get_z;
        const double z00 = get_z(map, ix, iy);
        const double z10 = get_z(map, ix + 1, iy);
        const double z01 = get_z(map, ix, iy + 1);
        const double z11 = get_z(map, ix + 1, iy + 1);

        if (hx <= 0.5) {
                const double gx1 = (z10 - z00) * (1. - hy) + (z11 - z01) * hy;
                if (ix == 0) {
                        *gx = gx1;
                } else {
                        const double z_10 = get_z(map, ix - 1, iy);
                        const double z_11 = get_z(map, ix - 1, iy + 1);
                        const double gx0 = (z00 - z_10) * (1. - hy) +
                            (z01 - z_11) * hy;
                        const double ax = hx + 0.5;
                        *gx = gx0 * (1. - ax) + gx1 * ax;
                }
        } else {
                const double gx0 = (z10 - z00) * (1. - hy) + (z11 - z01) * hy;
                if (ix == map->meta.nx - 2) {
                        *gx = gx0;
                } else {
                        const double z20 = get_z(map, ix + 2, iy);
                        const double z21 = get_z(map, ix + 2, iy + 1);
                        const double gx1 = (z20 - z10) * (1. - hy) +
                            (z21 - z11) * hy;
                        const double ax = hx - 0.5;
                        *gx = gx0 * (1. - ax) + gx1 * ax;
                }
        }

        if (hy <= 0.5) {
                const double gy1 = (z01 - z00) * (1. - hx) + (z11 - z10) * hx;
                if (iy == 0) {
                        *gx = gy1;
                } else {
                        const double z0_1 = get_z(map, ix, iy - 1);
                        const double z1_1 = get_z(map, ix + 1, iy - 1);
                        const double gy0 = (z00 - z0_1) * (1. - hx) +
                            (z10 - z1_1) * hx;
                        const double ay = hy + 0.5;
                        *gy = gy0 * (1. - ay) + gy1 * ay;
                }
        } else {
                const double gy0 = (z01 - z00) * (1. - hx) + (z11 - z10) * hx;
                if (iy == map->meta.ny - 2) {
                        *gy = gy0;
                } else {
                        const double z02 = get_z(map, ix, iy + 2);
                        const double z12 = get_z(map, ix + 1, iy + 2);
                        const double gy1 = (z02 - z01) * (1. - hx) +
                            (z12 - z11) * hx;
                        const double ay = hy - 0.5;
                        *gy = gy0 * (1. - ay) + gy1 * ay;
                }
        }

        if (inside != NULL) *inside = 1;
        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_map_elevation(
    const struct turtle_map * map, double x, double y, double * z, int * inside)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_elevation);
        return turtle_map_elevation_(map, x, y, z, inside, error_);
}

enum turtle_return turtle_map_gradient(const struct turtle_map * map,
    double x, double y, double * gx, double * gy, int * inside)
{
        TURTLE_ERROR_INITIALISE(&turtle_map_elevation);
        return turtle_map_gradient_(map, x, y, gx, gy, inside, error_);
}

const struct turtle_projection * turtle_map_projection(
    const struct turtle_map * map)
{
        if ((map == NULL) || (map->meta.projection.type < 0))
                return NULL;
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

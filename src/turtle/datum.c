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
 * Turtle datum handle for access to world-wide elevation data.
 */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tinydir.h"

#include "datum.h"
#include "error.h"
#include "loader.h"
#include "turtle.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* Create a new datum. */
enum turtle_return turtle_datum_create(const char * path, int stack_size,
    turtle_datum_cb * lock, turtle_datum_cb * unlock,
    struct turtle_datum ** datum)
{
        *datum = NULL;

        /* Check the lock and unlock consistency. */
        if (((lock == NULL) && (unlock != NULL)) ||
            ((unlock == NULL) && (lock != NULL)))
                return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_ADDRESS,
                    turtle_datum_create, "inconsistent lock & unlock");

        /* Scan the provided path for the tile data. */
        double lat_min = 1E+05, long_min = 1E+05;
        double lat_max = -1E+05, long_max = -1E+05;
        double lat_delta = 0., long_delta = 0.;
        int data_size = strlen(path) + 1;

        int rc;
        tinydir_dir dir;
        for (rc = tinydir_open(&dir, path); (rc == 0) && dir.has_next;
             tinydir_next(&dir)) {
                tinydir_file file;
                tinydir_readfile(&dir, &file);
                if (file.is_dir) continue;

                /* Check the format. */
                if (loader_format(file.path) == LOADER_FORMAT_UNKNOWN) continue;

                /* Get the tile meta-data. */
                struct datum_tile tile;
                if (loader_meta(file.path, &tile) != TURTLE_RETURN_SUCCESS)
                        continue;

                /* Update the lookup data. */
                const double dx = tile.dx * (tile.nx - 1);
                const double dy = tile.dy * (tile.ny - 1);
                if (long_delta == 0.)
                        long_delta = dx;
                else if (long_delta != dx)
                        goto format_error;
                if (lat_delta == 0.)
                        lat_delta = dy;
                else if (lat_delta != dy)
                        goto format_error;
                if (tile.x0 < long_min) long_min = tile.x0;
                if (tile.y0 < lat_min) lat_min = tile.y0;
                const double x1 = tile.x0 + dx;
                if (x1 > long_max) long_max = x1;
                const double y1 = tile.y0 + dy;
                if (y1 > lat_max) lat_max = y1;
                data_size += strlen(file.path) + 1;
        }
        if (rc == 0) tinydir_close(&dir);

        /* Check the grid size. */
        int lat_n = 0, long_n = 0;
        if ((lat_delta > 0.) && (long_delta > 0.)) {
                const double dx = (long_max - long_min) / long_delta;
                long_n = (int)(dx + FLT_EPSILON);
                if (fabs(long_n - dx) > FLT_EPSILON)
                        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_FORMAT,
                            turtle_datum_create, "invalid longitude grid");
                const double dy = (lat_max - lat_min) / lat_delta;
                lat_n = (int)(dy + FLT_EPSILON);
                if (fabs(lat_n - dy) > FLT_EPSILON)
                        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_FORMAT,
                            turtle_datum_create, "invalid latitude grid");
        }

        /* Allocate the new datum handle. */
        const int path_size = lat_n * long_n * sizeof(char *);
        data_size += path_size;
        *datum = malloc(sizeof(**datum) + data_size);
        if (*datum == NULL) return TURTLE_ERROR_MEMORY(turtle_datum_create);

        /* Initialise the handle. */
        (*datum)->stack = NULL;
        (*datum)->stack_size = 0;
        (*datum)->max_size = stack_size;
        (*datum)->lock = lock;
        (*datum)->unlock = unlock;
        (*datum)->latitude_0 = lat_min;
        (*datum)->longitude_0 = long_min;
        (*datum)->latitude_delta = lat_delta;
        (*datum)->longitude_delta = long_delta;
        (*datum)->latitude_n = lat_n;
        (*datum)->longitude_n = long_n;
        const int root_size = strlen(path) + 1;
        (*datum)->root = (char *)((*datum)->data);
        memcpy((*datum)->root, path, root_size);
        (*datum)->path = (char **)((*datum)->data + root_size);

        if ((lat_n == 0) || (long_n == 0)) return TURTLE_RETURN_SUCCESS;

        /* Build the lookup data. */
        int i;
        for (i = 0; i < lat_n * long_n; i++) (*datum)->path[i] = NULL;

        char * cursor = (*datum)->data + path_size;
        for (tinydir_open(&dir, path); dir.has_next; tinydir_next(&dir)) {
                tinydir_file file;
                tinydir_readfile(&dir, &file);
                if (file.is_dir) continue;

                /* Check the format. */
                if (loader_format(file.path) == LOADER_FORMAT_UNKNOWN) continue;

                /* Check the tile meta-data. */
                struct datum_tile tile;
                if (loader_meta(file.path, &tile) != TURTLE_RETURN_SUCCESS)
                        continue;

                /* Compute the lookup index. */
                const int ix = (int)((tile.x0 - long_min) / long_delta);
                const int iy = (int)((tile.y0 - lat_min) / lat_delta);
                i = iy * long_n + ix;

                /* Copy the path name. */
                const int n = strlen(file.path) + 1;
                memcpy(cursor, file.path, n);
                (*datum)->path[i] = cursor;
                cursor += n;
        }
        tinydir_close(&dir);

        return TURTLE_RETURN_SUCCESS;

format_error:
        tinydir_close(&dir);
        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_FORMAT,
            turtle_datum_create, "inconsistent elevation tiles");
}

/* Low level routine for cleaning the stack. */
void datum_clear(struct turtle_datum * datum, int force)
{
        struct datum_tile * tile = datum->stack;
        while (tile != NULL) {
                struct datum_tile * current = tile;
                tile = tile->prev;
                if ((force != 0) || (current->clients == 0))
                        datum_tile_destroy(datum, current);
        }
}

/* Destroy a datum and all its loaded tiles. */
void turtle_datum_destroy(struct turtle_datum ** datum)
{
        if ((datum == NULL) || (*datum == NULL)) return;

        /* Force the stack cleaning. */
        datum_clear(*datum, 1);

        /* Delete the datum and return. */
        free(*datum);
        *datum = NULL;
}

/* Clear the datum's stack from unused tiles. */
enum turtle_return turtle_datum_clear(struct turtle_datum * datum)
{
        if ((datum->lock != NULL) && (datum->lock() != 0))
                return TURTLE_ERROR_LOCK(turtle_datum_clear);

        /* Soft clean of the stack. */
        datum_clear(datum, 0);

        if ((datum->unlock != NULL) && (datum->unlock() != 0))
                return TURTLE_ERROR_UNLOCK(turtle_datum_clear);
        else
                return TURTLE_RETURN_SUCCESS;
}

/* Get the datum elevation at the given geodetic coordinates. */
enum turtle_return turtle_datum_elevation(struct turtle_datum * datum,
    double latitude, double longitude, double * elevation)
{
        /* Get the proper tile. */
        double hx, hy;
        int load = 1;
        if (datum->stack != NULL) {
                /* First let's check the top of the stack. */
                struct datum_tile * stack = datum->stack;
                hx = (longitude - stack->x0) / stack->dx;
                hy = (latitude - stack->y0) / stack->dy;

                if ((hx < 0.) || (hx > stack->nx) || (hy < 0.) ||
                    (hy > stack->ny)) {
                        /* The requested coordinates are not in the top
                         * tile. Let's check the full stack. */
                        struct datum_tile * tile = stack->prev;
                        while (tile != NULL) {
                                hx = (longitude - tile->x0) / tile->dx;
                                hy = (latitude - tile->y0) / tile->dy;

                                if ((hx >= 0.) && (hx <= tile->nx) &&
                                    (hy >= 0.) && (hy <= tile->ny)) {
                                        /*
                                         * Move the valid tile to the top
                                         * of the stack.
                                         */
                                        datum_tile_touch(datum, tile);
                                        load = 0;
                                        break;
                                }
                                tile = tile->prev;
                        }
                } else
                        load = 0;
        }

        if (load) {
                /* No valid tile was found. Let's try to load it. */
                enum turtle_return rc =
                    datum_tile_load(datum, latitude, longitude);
                if (rc == TURTLE_RETURN_MEMORY_ERROR) {
                        return TURTLE_ERROR_MEMORY(turtle_datum_elevation);
                } else if (rc == TURTLE_RETURN_PATH_ERROR) {
                        return TURTLE_ERROR_MISSING_DATA(
                            turtle_datum_elevation, datum);
                } else if (rc != TURTLE_RETURN_SUCCESS) {
                        return TURTLE_ERROR_UNEXPECTED(
                            rc, turtle_datum_elevation);
                }

                struct datum_tile * stack = datum->stack;
                hx = (longitude - stack->x0) / stack->dx;
                hy = (latitude - stack->y0) / stack->dy;
        }

        /* Interpolate the elevation. */
        int ix = (int)hx;
        int iy = (int)hy;
        hx -= ix;
        hy -= iy;
        const int nx = datum->stack->nx;
        const int ny = datum->stack->ny;
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        int ix1 = (ix >= nx - 1) ? nx - 1 : ix + 1;
        int iy1 = (iy >= ny - 1) ? ny - 1 : iy + 1;
        struct datum_tile * tile = datum->stack;
        datum_tile_cb * zm = tile->z;
        *elevation = zm(tile, ix, iy) * (1. - hx) * (1. - hy) +
            zm(tile, ix, iy1) * (1. - hx) * hy +
            zm(tile, ix1, iy) * hx * (1. - hy) + zm(tile, ix1, iy1) * hx * hy;
        return TURTLE_RETURN_SUCCESS;
}

/* Ellipsoid parameters  */
#define WGS84_A 6378137
#define WGS84_E 0.081819190842622

/* Compute ECEF coordinates from latitude and longitude. */
void turtle_datum_ecef(struct turtle_datum * datum, double latitude,
    double longitude, double elevation, double ecef[3])
{
        /* Get the parameters of the reference ellipsoid. */
        const double a = WGS84_A, e = WGS84_E;

        /* Compute the Cartesian coordinates. */
        const double s = sin(latitude * M_PI / 180.);
        const double c = cos(latitude * M_PI / 180.);
        const double R = a / sqrt(1. - e * e * s * s);

        ecef[0] = (R + elevation) * c * cos(longitude * M_PI / 180.);
        ecef[1] = (R + elevation) * c * sin(longitude * M_PI / 180.);
        ecef[2] = (R * (1. - e * e) + elevation) * s;
}

/* Compute the geodetic coordinates from the ECEF ones.
 *
 * Reference: B. R. Bowring's 1985 algorithm (single iteration).
 */
void turtle_datum_geodetic(struct turtle_datum * datum, double ecef[3],
    double * latitude, double * longitude, double * elevation)
{
        /* Get the parameters of the reference ellipsoid. */
        const double a = WGS84_A, e = WGS84_E;
        const double b2 = a * a * (1. - e * e);
        const double b = sqrt(b2);
        const double eb2 = e * e * a * a / b2;

        /* Compute the geodetic coordinates. */
        if ((ecef[0] == 0.) && (ecef[1] == 0.)) {
                *latitude = (ecef[2] >= 0.) ? 90. : -90.;
                *longitude = 0.0;
                *elevation = fabs(ecef[2]) - b;
                return;
        }

        *longitude = atan2(ecef[1], ecef[0]) * 180. / M_PI;

        const double p2 = ecef[0] * ecef[0] + ecef[1] * ecef[1];
        const double p = sqrt(p2);
        if (ecef[2] == 0.) {
                *latitude = 0.;
                *elevation = p - a;
                return;
        }

        const double r = sqrt(p2 + ecef[2] * ecef[2]);
        const double tu = b * ecef[2] * (1. + eb2 * b / r) / (a * p);
        const double tu2 = tu * tu;
        const double cu = 1. / sqrt(1. + tu2);
        const double su = cu * tu;
        const double tp =
            (ecef[2] + eb2 * b * su * su * su) / (p - e * e * a * cu * cu * cu);
        *latitude = atan(tp) * 180. / M_PI;
        const double cp = 1. / sqrt(1.0 + tp * tp);
        const double sp = cp * tp;
        *elevation = p * cp + ecef[2] * sp - a * sqrt(1. - e * e * sp * sp);
}

/* Compute the local East, North, Up (ENU) basis vectors. .
 *
 * Reference: https://en.wikipedia.org/wiki/Horizontal_coordinate_system.
 */
static inline void compute_enu(
    double latitude, double longitude, double e[3], double n[3], double u[3])
{
        const double lambda = longitude * M_PI / 180.;
        const double phi = latitude * M_PI / 180.;
        const double sl = sin(lambda);
        const double cl = cos(lambda);
        const double sp = sin(phi);
        const double cp = cos(phi);
        e[0] = -sl;
        e[1] = cl;
        e[2] = 0.;
        n[0] = -cl * sp;
        n[1] = -sl * sp;
        n[2] = cp;
        u[0] = cl * cp;
        u[1] = sl * cp;
        u[2] = sp;
}

/* Compute the direction vector in ECEF from the horizontal coordinates.
 *
 * Reference: https://en.wikipedia.org/wiki/Horizontal_coordinate_system.
 */
void turtle_datum_direction(struct turtle_datum * datum, double latitude,
    double longitude, double azimuth, double elevation, double direction[3])
{
        /* Compute the local E, N, U basis vectors. */
        double e[3], n[3], u[3];
        compute_enu(latitude, longitude, e, n, u);

        /* Project on the E,N,U basis. */
        const double az = azimuth * M_PI / 180.;
        const double el = elevation * M_PI / 180.;
        const double ce = cos(el);
        const double r[3] = { ce * sin(az), ce * cos(az), sin(el) };

        direction[0] = r[0] * e[0] + r[1] * n[0] + r[2] * u[0];
        direction[1] = r[0] * e[1] + r[1] * n[1] + r[2] * u[1];
        direction[2] = r[0] * e[2] + r[1] * n[2] + r[2] * u[2];
}

enum turtle_return turtle_datum_horizontal(struct turtle_datum * datum,
    double latitude, double longitude, double direction[3], double * azimuth,
    double * elevation)
{
        /* Compute the local E, N, U basis vectors. */
        double e[3], n[3], u[3];
        compute_enu(latitude, longitude, e, n, u);

        /* Project on the E,N,U basis. */
        const double x =
            e[0] * direction[0] + e[1] * direction[1] + e[2] * direction[2];
        const double y =
            n[0] * direction[0] + n[1] * direction[1] + n[2] * direction[2];
        const double z =
            u[0] * direction[0] + u[1] * direction[1] + u[2] * direction[2];
        double r = direction[0] * direction[0] + direction[1] * direction[1] +
            direction[2] * direction[2];
        if (r <= 0.)
                return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_DOMAIN_ERROR,
                    turtle_datum_horizontal, "null direction vector");
        r = sqrt(r);
        *azimuth = atan2(x, y) * 180. / M_PI;
        *elevation = asin(z / r) * 180. / M_PI;

        return TURTLE_RETURN_SUCCESS;
}

/* Move a tile to the top of the stack. */
void datum_tile_touch(struct turtle_datum * datum, struct datum_tile * tile)
{
        if (tile->next == NULL) return; /* Already on top. */
        struct datum_tile * prev = tile->prev;
        tile->next->prev = prev;
        if (prev != NULL) prev->next = tile->next;
        datum->stack->next = tile;
        tile->prev = datum->stack;
        tile->next = NULL;
        datum->stack = tile;
}

/* Remove a tile from the stack. */
void datum_tile_destroy(struct turtle_datum * datum, struct datum_tile * tile)
{
        struct datum_tile * prev = tile->prev;
        struct datum_tile * next = tile->next;
        if (prev != NULL) prev->next = next;
        if (next != NULL)
                next->prev = prev;
        else
                datum->stack = prev;
        free(tile);
        datum->stack_size--;
}

/* Load a new tile and manage the stack. */
enum turtle_return datum_tile_load(
    struct turtle_datum * datum, double latitude, double longitude)
{
        /* Lookup the requested file. */
        if ((longitude < datum->longitude_0) || (latitude < datum->latitude_0))
                return TURTLE_RETURN_PATH_ERROR;
        const int ix =
            (int)((longitude - datum->longitude_0) / datum->longitude_delta);
        if (ix >= datum->longitude_n) return TURTLE_RETURN_PATH_ERROR;
        const int iy =
            (int)((latitude - datum->latitude_0) / datum->latitude_delta);
        if (iy >= datum->latitude_n) return TURTLE_RETURN_PATH_ERROR;
        const int index = iy * datum->longitude_n + ix;
        if (datum->path[index] == NULL) return TURTLE_RETURN_PATH_ERROR;

        /* Load the tile data according to the format. */
        struct datum_tile * tile = NULL;
        enum turtle_return rc;
        if ((rc = loader_load(datum->path[index], &tile)) !=
            TURTLE_RETURN_SUCCESS)
                return rc;

        /* Initialise the client's references. */
        tile->clients = 0;

        /* Make room for the new tile, if needed. */
        if (datum->stack_size >= datum->max_size) {
                struct datum_tile * last = datum->stack;
                while (last->prev != NULL) last = last->prev;
                while (
                    (last != NULL) && (datum->stack_size >= datum->max_size)) {
                        struct datum_tile * current = last;
                        last = last->next;
                        if (current->clients == 0)
                                datum_tile_destroy(datum, current);
                }
        }

        /* Append the new tile on top of the stack. */
        tile->next = NULL;
        tile->prev = datum->stack;
        if (datum->stack != NULL) datum->stack->next = tile;
        datum->stack = tile;
        datum->stack_size++;

        return TURTLE_RETURN_SUCCESS;
}

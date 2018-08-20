/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRacking The eLEvation (TURTLE)
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
 * I/O's for grd files providing a reader for text grids, e.g. EGM96 geoid
 */

/* C89 standard library */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Endianess utilities */
#include <arpa/inet.h>
/* TURTLE library */
#include "turtle/io.h"

/* Data for accessing a grd file */
struct grd_io {
        /* Base io object */
        struct turtle_io base;

        /* Internal data for the io */
        FILE * fid;
        const char * path;
};

static enum turtle_return grd_open(struct turtle_io * io, const char * path,
    const char * mode, struct turtle_error_context * error_)
{
        struct grd_io * grd = (struct grd_io *)io;
        if (grd->fid != NULL) io->close(io);

        if (mode[0] != 'r') {
                /* Write mode is not yet implemented */
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "invalid write format for file `%s'", path);
        }

        /* Initialise the io object */
        io->meta.nx = io->meta.ny = 0;
        io->meta.x0 = io->meta.y0 = io->meta.z0 = 0.;
        io->meta.dx = io->meta.dy = io->meta.dz = 0.;
        io->meta.projection.type = PROJECTION_NONE;
        grd->path = NULL;

        /* Open the file */
        grd->fid = fopen(path, "r");
        if (grd->fid == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }
        grd->path = path;

        /* Parse the meta data from the header */
        double h[6];
        if (fscanf(grd->fid, "%lf %lf %lf %lf %lf %lf", h, h + 1, h + 2, h + 3,
            h + 4, h + 5) != 6) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "could not read the header of file `%s'", path);
        }
        io->meta.x0 = h[2];
        io->meta.dx = h[5];
        io->meta.y0 = h[0];
        io->meta.dy = h[4];
        io->meta.nx = (int)round((h[3] - h[2]) / h[5]) + 1;
        io->meta.ny = (int)round((h[1] - h[0]) / h[4]) + 1;

        /* Check the min and max z values */
        const long offset = ftell(grd->fid);
        int i;
        double zmin = DBL_MAX, zmax = -DBL_MIN;
        for (i = 0; i < io->meta.ny; i++) {
                int j;
                for (j = 0; j < io->meta.nx; j++) {
                        double d;
                        if (fscanf(grd->fid, "%lf", &d) != 1) {
                                io->close(io);
                                return TURTLE_ERROR_VREGISTER(
                                    TURTLE_RETURN_BAD_FORMAT,
                                    "inconsitent data in file `%s'", path);
                        }
                        if (d < zmin)
                                zmin = d;
                        else if (d > zmax)
                                zmax = d;
                }
        }
        fseek(grd->fid, offset, SEEK_SET);
        io->meta.z0 = zmin;
        io->meta.dz = (zmax - zmin) / 65535;

        return TURTLE_RETURN_SUCCESS;
}

static void grd_close(struct turtle_io * io)
{
        struct grd_io * grd = (struct grd_io *)io;
        if (grd->fid != NULL) {
                fclose(grd->fid);
                grd->fid = NULL;
                grd->path = NULL;
        }
}

static double get_z(const struct turtle_map * map, int ix, int iy)
{
        const uint16_t iz = (uint16_t)map->data[iy * map->meta.nx + ix];
        return map->meta.z0 + iz * map->meta.dz;
}

static void set_z(struct turtle_map * map, int ix, int iy, double z)
{
        const double d = round((z - map->meta.z0) / map->meta.dz);
        map->data[iy * map->meta.nx + ix] = (uint16_t)d;
}

static enum turtle_return grd_read(struct turtle_io * io,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct grd_io * grd = (struct grd_io *)io;

        int i = 0;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), grd->fid) != NULL) {
                char * start = buffer, * end;
                for (;; i++) {
                        const double d = strtod(start, &end);
                        if (start == end) break;
                        start = end;
                        int ix  = i % io->meta.nx;
                        int iy = i / io->meta.nx;
                        set_z(map, ix, iy, d);
                }
        }

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_io_grd_create_(
    struct turtle_io ** io_p, struct turtle_error_context * error_)
{
        /* Allocate the grd io manager */
        struct grd_io * grd = malloc(sizeof(*grd));
        if (grd == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for grd format");
        }
        *io_p = &grd->base;

        /* Initialise the io object */
        memset(grd, 0x0, sizeof(*grd));
        grd->fid = NULL;
        grd->path = NULL;
        grd->base.meta.projection.type = PROJECTION_NONE;

        grd->base.open = &grd_open;
        grd->base.close = &grd_close;
        grd->base.read = &grd_read;
        grd->base.write = NULL;

        grd->base.meta.get_z = &get_z;
        grd->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}

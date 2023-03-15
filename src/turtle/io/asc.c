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
 * I/O's for asc files providing a reader for text grids, e.g. GEBCO bathymetry
 */

/* C89 standard library */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TURTLE library */
#include "turtle/io.h"

/* Data for accessing a asc file */
struct asc_io {
        /* Base io object */
        struct turtle_io base;

        /* Internal data for the io */
        FILE * fid;
        const char * path;
};

static enum turtle_return asc_open(struct turtle_io * io, const char * path,
    const char * mode, struct turtle_error_context * error_)
{
        struct asc_io * asc = (struct asc_io *)io;
        if (asc->fid != NULL) io->close(io);

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
        asc->path = NULL;

        /* Open the file */
        asc->fid = fopen(path, "r");
        if (asc->fid == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }
        asc->path = path;

        /* Parse the meta data from the header */
        double nodata;
        if ((fscanf(asc->fid, "%*s %d",  &io->meta.nx) != 1) ||
            (fscanf(asc->fid, "%*s %d",  &io->meta.ny) != 1) ||
            (fscanf(asc->fid, "%*s %lf", &io->meta.x0) != 1) ||
            (fscanf(asc->fid, "%*s %lf", &io->meta.y0) != 1) ||
            (fscanf(asc->fid, "%*s %lf", &io->meta.dx) != 1) ||
            (fscanf(asc->fid, "%*s %lf", &nodata) != 1)) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "could not read the header of file `%s'", path);
        }
        io->meta.dy = io->meta.dx;
        io->meta.x0 += 0.5 * io->meta.dx;
        io->meta.y0 += 0.5 * io->meta.dy;

        /* Check the min and max z values */
        const long offset = ftell(asc->fid);
        int i;
        double zmin = DBL_MAX, zmax = -DBL_MIN;
        for (i = 0; i < io->meta.ny; i++) {
                int j;
                for (j = 0; j < io->meta.nx; j++) {
                        double d;
                        if (fscanf(asc->fid, "%lf", &d) != 1) {
                                io->close(io);
                                return TURTLE_ERROR_VREGISTER(
                                    TURTLE_RETURN_BAD_FORMAT,
                                    "inconsistent data in file `%s'", path);
                        }
                        if (d == nodata)
                                continue;
                        else if (d < zmin)
                                zmin = d;
                        else if (d > zmax)
                                zmax = d;
                }
        }
        fseek(asc->fid, offset, SEEK_SET);
        io->meta.z0 = zmin;
        io->meta.dz = (zmax - zmin) / 65535;

        return TURTLE_RETURN_SUCCESS;
}

static void asc_close(struct turtle_io * io)
{
        struct asc_io * asc = (struct asc_io *)io;
        if (asc->fid != NULL) {
                fclose(asc->fid);
                asc->fid = NULL;
                asc->path = NULL;
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

static enum turtle_return asc_read(struct turtle_io * io,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct asc_io * asc = (struct asc_io *)io;
        int ix, iy;
        for (iy = io->meta.ny - 1; iy >= 0; iy--) /* data in reading order */
            for (ix = 0; ix < io->meta.nx; ix++) {
                double d;
                if (fscanf(asc->fid, "%lf", &d) != 1) d = 0.;
                set_z(map, ix, iy, d);
        }

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_io_asc_create_(
    struct turtle_io ** io_p, struct turtle_error_context * error_)
{
        /* Allocate the asc io manager */
        struct asc_io * asc = malloc(sizeof(*asc));
        if (asc == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for asc format");
        }
        *io_p = &asc->base;

        /* Initialise the io object */
        memset(asc, 0x0, sizeof(*asc));
        asc->fid = NULL;
        asc->path = NULL;
        asc->base.meta.projection.type = PROJECTION_NONE;

        asc->base.open = &asc_open;
        asc->base.close = &asc_close;
        asc->base.read = &asc_read;
        asc->base.write = NULL;

        asc->base.meta.get_z = &get_z;
        asc->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}

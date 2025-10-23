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

static void upify(char * s)
{
        while (*s != '\0') {
                if ((*s >= 'a') && (*s <= 'z')) {
                        *s += 'A' - 'a';
                }
                s++;
        }
}

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
        char ncols[8] = { 0 }, nrows[8] = { 0 };
        char x0entry[16] = { 0 }, y0entry[16] = { 0 }, cellsize[16] = { 0 };
        if ((fscanf(asc->fid, "%7s %d", ncols, &io->meta.nx) != 2) ||
            (fscanf(asc->fid, "%7s %d", nrows, &io->meta.ny) != 2) ||
            (fscanf(asc->fid, "%15s %lf", x0entry, &io->meta.x0) != 2) ||
            (fscanf(asc->fid, "%15s %lf", y0entry, &io->meta.y0) != 2) ||
            (fscanf(asc->fid, "%15s %lf", cellsize, &io->meta.dx) != 2)) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "could not read the header of file `%s'", path);
        }
        io->meta.dy = io->meta.dx;
        upify(ncols);
        if (strcmp(ncols, "NCOLS") != 0) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT,
                    "%s: expected `NCOLS`, found `%s`",
                    path,
                    ncols
                );
        }
        upify(nrows);
        if (strcmp(nrows, "NROWS") != 0) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT,
                    "%s: expected `NROWS`, found `%s`",
                    path,
                    nrows
                );
        }
        upify(x0entry);
        if (strcmp(x0entry, "XLLCORNER") == 0) {
                io->meta.x0 += 0.5 * io->meta.dx;
        } else if (strcmp(x0entry, "XLLCENTER") != 0) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT,
                    "%s: expected `XLLCENTER` or `XLLCORNER`, found `%s`",
                    path,
                    x0entry
                );
        }
        upify(y0entry);
        if (strcmp(y0entry, "YLLCORNER") == 0) {
                io->meta.y0 += 0.5 * io->meta.dy;
        } else if (strcmp(y0entry, "YLLCENTER") != 0) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT,
                    "%s: expected `YLLCENTER` or `YLLCORNER`, found `%s`",
                    path,
                    y0entry
                );
        }
        upify(cellsize);
        if (strcmp(cellsize, "CELLSIZE") != 0) {
                io->close(io);
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT,
                    "%s: expected `CELLSIZE`, found `%s`",
                    path,
                    cellsize
                );
        }

        /* Check for any nodata_value */
        long offset = ftell(asc->fid);
        double nodata_value = -DBL_MAX;
        double zmin = DBL_MAX, zmax = -DBL_MAX;
        int i = 0;
        {
                double d;
                int nread = fscanf(asc->fid, "%lf", &d);
                if (nread  == 0) {
                        char nodata[32] = { 0 };
                        fseek(asc->fid, offset, SEEK_SET);
                        if (fscanf(asc->fid, "%31s %lf", nodata,
                             &nodata_value) != 2) {
                                io->close(io);
                                return TURTLE_ERROR_VREGISTER(
                                    TURTLE_RETURN_BAD_FORMAT,
                                    "could not read the header of file `%s'",
                                    path
                                );
                        }
                        offset = ftell(asc->fid);
                        upify(nodata);
                        if (strcmp(nodata, "NODATA_VALUE") != 0) {
                                io->close(io);
                                return TURTLE_ERROR_VREGISTER(
                                    TURTLE_RETURN_BAD_FORMAT,
                                    "%s: expected `NODATA_VALUE`, found `%s`",
                                    path,
                                    nodata
                                );
                        }
                } else if (nread == 1) {
                        if (d != nodata_value) {
                                zmin = zmax = d;
                        }
                        i++;
                } else {
                        io->close(io);
                        return TURTLE_ERROR_VREGISTER(
                            TURTLE_RETURN_BAD_FORMAT,
                            "inconsistent data in file `%s'",
                            path
                        );
                }
        }

        /* Check the min and max z values */
        const int n = io->meta.nx * io->meta.ny;
        for (; i < n; i++) {
                double d;
                if (fscanf(asc->fid, "%lf", &d) != 1) {
                        io->close(io);
                        return TURTLE_ERROR_VREGISTER(
                            TURTLE_RETURN_BAD_FORMAT,
                            "inconsistent data in file `%s'",
                            path
                        );
                }
                if (d == nodata_value) {
                        continue;
                } else {
                        if (d < zmin) zmin = d;
                        if (d > zmax) zmax = d;
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
        double d = round((z - map->meta.z0) / map->meta.dz);
        if (d < 0.0) d = 0.0;
        else if (d > 65535.0) d = 65535.0;
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

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
 * I/O's for geotiff files providing a reader for 16b data, e.g. ASTER-GDEM2
 * or SRTM tiles
 */

/* C89 standard library */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef TURTLE_NO_LD
/* Dynamic libraries */
#include <dlfcn.h>
#endif
/* TIFF library */
#ifndef TURTLE_NO_LD
#include "deps/tiffio.h"
#else
#include <tiffio.h>
#endif
/* TURTLE library */
#include "turtle/io.h"

/* GEOTIFF tags */
#define TIFFTAG_GEOPIXELSCALE 33550
#define TIFFTAG_INTERGRAPH_MATRIX 33920
#define TIFFTAG_GEOTIEPOINTS 33922
#define TIFFTAG_GEOKEYDIRECTORY 34735
#define TIFFTAG_GEODOUBLEPARAMS 34736
#define TIFFTAG_GEOASCIIPARAMS 34737
#define TIFFTAG_GDAL_METADATA 42112
#define TIFFTAG_GDAL_NODATA 42113

static const TIFFFieldInfo field_info[] = { { TIFFTAG_GEOPIXELSCALE, -1, -1,
                                                TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
                                                "GeoPixelScale" },
        { TIFFTAG_INTERGRAPH_MATRIX, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
            "Intergraph TransformationMatrix" },
        { TIFFTAG_GEOTIEPOINTS, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
            "GeoTiePoints" },
        { TIFFTAG_GEOKEYDIRECTORY, -1, -1, TIFF_SHORT, FIELD_CUSTOM, 1, 1,
            "GeoKeyDirectory" },
        { TIFFTAG_GEODOUBLEPARAMS, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
            "GeoDoubleParams" },
        { TIFFTAG_GEOASCIIPARAMS, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0,
            "GeoASCIIParams" },
        { TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0,
            "GDAL_METADATA" },
        { TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0,
            "GDAL_NODATA" } };

/* Extender for GEOTIFF tags */
static TIFFExtendProc parent_extender = NULL;

/* The TIFF API */
static struct {
        void * lib;

        TIFF * (*Open) (const char *, const char *);
        TIFFErrorHandler (*SetErrorHandler) (TIFFErrorHandler);
        TIFFExtendProc (*SetTagExtender) (TIFFExtendProc);
        int (*GetField) (TIFF *, ttag_t, ...);
        int (*MergeFieldInfo) (TIFF *, const TIFFFieldInfo[], uint32);
        int (*ReadScanline) (TIFF *, tdata_t, uint32, tsample_t);
        int (*SetField) (TIFF *, ttag_t, ...);
        int (*WriteScanline) (TIFF *, tdata_t, uint32, tsample_t);
        tsize_t (*ScanlineSize) (TIFF *);
        void (*Close) (TIFF *);
} api;

static void default_directory(TIFF * tiff)
{
        api.MergeFieldInfo(
            tiff, field_info, sizeof(field_info) / sizeof(field_info[0]));
        if (parent_extender != NULL) (*parent_extender)(tiff);
}

static enum turtle_return api_initialise(struct turtle_error_context * error_)
{
#ifndef TURTLE_NO_LD
#ifdef __APPLE__
#define SOEXT "dylib"
#else
#define SOEXT "so"
#endif
        if (api.lib != NULL)
                return TURTLE_RETURN_SUCCESS;

        api.lib = dlopen("libtiff." SOEXT, RTLD_LAZY);
        if (api.lib == NULL) {
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_PATH_ERROR, dlerror());
        }

#define LINK(NAME)                                                             \
        if ((api. NAME = dlsym(api.lib, "TIFF" #NAME)) == NULL)                \
                goto error
#else
        static int initialised = 0;
        if (initialised) return TURTLE_RETURN_SUCCESS;
        initialised = 1;

#define LINK(NAME) api. NAME = (void *)&TIFF ##NAME
#endif

        LINK(Open);
        LINK(SetErrorHandler);
        LINK(SetTagExtender);
        LINK(GetField);
        LINK(MergeFieldInfo);
        LINK(ReadScanline);
        LINK(SetField);
        LINK(WriteScanline);
        LINK(ScanlineSize);
        LINK(Close);

        /* Register the tag extender to libtiff */
        parent_extender = api.SetTagExtender(default_directory);
        api.SetErrorHandler(NULL); /* Mute error messages */

        return TURTLE_RETURN_SUCCESS;

#ifndef TURTLE_NO_LD
error:
        dlclose(api.lib);
        api.lib = NULL;
        return TURTLE_ERROR_REGISTER(TURTLE_RETURN_BAD_FORMAT, dlerror());

#undef LINK
#undef SOEXT
#endif
}

/* Data for accessing a GEOTIFF file */
struct geotiff16_io {
        /* Base io object */
        struct turtle_io base;

        /* Internal data for the GEOTIFF format */
        TIFF * tiff;
        const char * path;
};

static enum turtle_return geotiff16_open(struct turtle_io * io,
    const char * path, const char * mode, struct turtle_error_context * error_)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;
        if (geotiff16->tiff == NULL) io->close(io);

        if (mode[0] != 'r') {
                geotiff16->tiff = api.Open(path, mode);
                if (geotiff16->tiff == NULL) {
                        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_PATH_ERROR,
                            "could not create file `%s'", path);
                }

                geotiff16->path = path;
                return EXIT_SUCCESS;
        }

        /* Initialise the io object */
        io->meta.nx = io->meta.ny = 0;
        io->meta.x0 = io->meta.y0 = 0.;
        io->meta.dx = io->meta.dy = 0.;
        io->meta.z0 = -32767.;
        io->meta.dz = 1.;
        io->meta.projection.type = PROJECTION_NONE;

        /* Open the TIFF file */
        geotiff16->tiff = api.Open(path, "r");
        if (geotiff16->tiff == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }

        /* Initialise the new io and return */
        uint32_t height;
        api.GetField(geotiff16->tiff, TIFFTAG_IMAGELENGTH, &height);
        io->meta.ny = height;
        tsize_t size = api.ScanlineSize(geotiff16->tiff);
        io->meta.nx = size / sizeof(int16);
        int count = 0;
        double * data = NULL;
        api.GetField(geotiff16->tiff, TIFFTAG_GEOPIXELSCALE, &count, &data);
        if (count == 3) {
                io->meta.dx = data[0];
                io->meta.dy = data[1];
        }
        api.GetField(geotiff16->tiff, TIFFTAG_GEOTIEPOINTS, &count, &data);
        if (count == 6) {
                io->meta.x0 = data[3];
                io->meta.y0 = data[4] + (1 - io->meta.ny) * io->meta.dy;
        }

        geotiff16->path = path;
        return TURTLE_RETURN_SUCCESS;
}

static void geotiff16_close(struct turtle_io * io)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;
        if (geotiff16->tiff != NULL) {
                api.Close(geotiff16->tiff);
                geotiff16->tiff = NULL;
                geotiff16->path = NULL;
        }
}

static double get_z(const struct turtle_map * map, int ix, int iy)
{
        return (int16_t)map->data[iy * map->meta.nx + ix];
}

static void set_z(struct turtle_map * map, int ix, int iy, double z)
{
        map->data[iy * map->meta.nx + ix] = (int16_t)z;
}

static enum turtle_return geotiff16_read(struct turtle_io * io,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;

        /* Unpack the data */
        uint16_t * buffer = map->data;
        buffer += io->meta.nx * (io->meta.ny - 1);
        int i;
        for (i = 0; i < io->meta.ny; i++, buffer -= io->meta.nx) {
                if (api.ReadScanline(geotiff16->tiff, buffer, i, 0) != 1) {
                        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                            "a libtiff error occured when reading file `%s'",
                            geotiff16->path);
                }
        }

        return TURTLE_RETURN_SUCCESS;
}

/* Dump a map in GEOTIFF format */
static enum turtle_return geotiff16_write(struct turtle_io * io,
    const struct turtle_map * map, struct turtle_error_context * error_)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;

        /* Check the map scale */
        if ((map->meta.z0 != -32767.) || (map->meta.dz != 1.)) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "unsupported z scale when dumping map to `%s'",
                    geotiff16->path);
        }

        /* Check that there is no projection */
        if (turtle_map_projection((struct turtle_map *)map) != NULL) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "unsupported projection when dumping map to `%s'",
                    geotiff16->path);
        }

        /* Dump the meta data */
        api.SetField(geotiff16->tiff, TIFFTAG_IMAGEWIDTH, map->meta.nx);
        api.SetField(geotiff16->tiff, TIFFTAG_IMAGELENGTH, map->meta.ny);
        api.SetField(geotiff16->tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
        api.SetField(geotiff16->tiff, TIFFTAG_BITSPERSAMPLE, 16);
        api.SetField(geotiff16->tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        api.SetField(
            geotiff16->tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        api.SetField(geotiff16->tiff, TIFFTAG_ROWSPERSTRIP, 1);
        api.SetField(
            geotiff16->tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        api.SetField(geotiff16->tiff, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
        api.SetField(geotiff16->tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);

        double scale[3] = { map->meta.dx, map->meta.dy, 0. };
        api.SetField(geotiff16->tiff, TIFFTAG_GEOPIXELSCALE, 3, scale);

        double tiepoints[6] = { 0, 0, 0, map->meta.x0,
            map->meta.y0 + (map->meta.ny - 1) * map->meta.dy, 0. };
        api.SetField(geotiff16->tiff, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);

        /* Dump the raw data */
        int16_t * data = malloc(map->meta.nx * sizeof(*data));
        if (data == NULL) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory when writing file `%s'",
                    geotiff16->path);
        }

        int i;
        for (i = 0; i < map->meta.ny; i++) {
                int j;
                for (j = 0; j < map->meta.nx; j++) {
                        const double d = round(map->meta.get_z(map, j, i));
                        data[j] = (int16_t)d;
                }
                if (api.WriteScanline(geotiff16->tiff, data, i, 0) != 1) {
                        free(data);
                        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                            "a libtiff error occured when writing to file `%s'",
                            geotiff16->path);
                }
        }
        free(data);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_io_geotiff16_create_(
    struct turtle_io ** io_p, struct turtle_error_context * error_)
{
        enum turtle_return rc;
        if ((rc = api_initialise(error_)) != TURTLE_RETURN_SUCCESS)
                return rc;

        /* Allocate the geotiff16 io manager */
        struct geotiff16_io * geotiff16 = malloc(sizeof(*geotiff16));
        if (geotiff16 == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for geotiff16 format");
        }
        *io_p = &geotiff16->base;

        /* Initialise the io object */
        memset(geotiff16, 0x0, sizeof(*geotiff16));
        geotiff16->tiff = NULL;
        geotiff16->path = NULL;
        geotiff16->base.meta.projection.type = PROJECTION_NONE;

        geotiff16->base.open = &geotiff16_open;
        geotiff16->base.close = &geotiff16_close;
        geotiff16->base.read = &geotiff16_read;
        geotiff16->base.write = &geotiff16_write;

        geotiff16->base.meta.get_z = &get_z;
        geotiff16->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}

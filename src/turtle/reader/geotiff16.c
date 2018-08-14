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
 * Interface to geotiff files providing a reader for 16b data, e.g. ASTER-GDEM2
 * or SRTM tiles
 */

/* C89 standard library */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TIFF library */
#include <tiffio.h>
/* TURTLE library */
#include "turtle/reader.h"

/* GEOTIFF tags */
#define TIFFTAG_GEOPIXELSCALE 33550
#define TIFFTAG_INTERGRAPH_MATRIX 33920
#define TIFFTAG_GEOTIEPOINTS 33922
#define TIFFTAG_GEOKEYDIRECTORY 34735
#define TIFFTAG_GEODOUBLEPARAMS 34736
#define TIFFTAG_GEOASCIIPARAMS 34737
#define TIFFTAG_GDAL_METADATA 42112
#define TIFFTAG_GDAL_NODATA 42113

/* Data for reading a png file */
struct geotiff16_reader {
        /* Base reader object */
        struct turtle_reader base;

        /* Internal data for the reader */
        TIFF * tiff;
        const char * path;
};

static enum turtle_return geotiff16_open(struct turtle_reader * reader,
    const char * path, struct turtle_error_context * error_)
{
        struct geotiff16_reader * geotiff16 =
            (struct geotiff16_reader *)reader;
        if (geotiff16->tiff == NULL) reader->close(reader);

        /* Initialise the reader object */
        reader->meta.nx = reader->meta.ny = 0;
        reader->meta.x0 = reader->meta.y0 = reader->meta.z0 = 0.;
        reader->meta.dx = reader->meta.dy = reader->meta.dz = 0.;
        reader->meta.projection.type = PROJECTION_NONE;

        /* Open the TIFF file. */
        geotiff16->tiff = TIFFOpen(path, "r");
        if (geotiff16->tiff == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }

        /* Initialise the new reader and return. */
        uint32_t height;
        TIFFGetField(geotiff16->tiff, TIFFTAG_IMAGELENGTH, &height);
        reader->meta.ny = height;
        tsize_t size = TIFFScanlineSize(geotiff16->tiff);
        reader->meta.nx = size / sizeof(int16);
        int count = 0;
        double * data = NULL;
        TIFFGetField(geotiff16->tiff, TIFFTAG_GEOPIXELSCALE, &count, &data);
        if (count == 3) {
                reader->meta.dx = data[0];
                reader->meta.dy = data[1];
                reader->meta.dz = data[2];
        }
        TIFFGetField(geotiff16->tiff, TIFFTAG_GEOTIEPOINTS, &count, &data);
        if (count == 6) {
                reader->meta.x0 = data[3];
                reader->meta.y0 =
                    data[4] + (1. - reader->meta.ny) * reader->meta.dy;
                reader->meta.z0 = data[5];
        }

        geotiff16->path = path;
        return TURTLE_RETURN_SUCCESS;
}

static void geotiff16_close(struct turtle_reader * reader)
{
        struct geotiff16_reader * geotiff16 =
            (struct geotiff16_reader *)reader;
        if (geotiff16->tiff != NULL) {
                TIFFClose(geotiff16->tiff);
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
        map->data[iy * map->meta.nx + ix] = (uint16_t)(int16_t)z;
}

static enum turtle_return geotiff16_read(struct turtle_reader * reader,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct geotiff16_reader * geotiff16 =
            (struct geotiff16_reader *)reader;

        /* Unpack the data */
        uint16_t * buffer = map->data;
        buffer += reader->meta.nx * (reader->meta.ny - 1);
        int i;
        for (i = 0; i < reader->meta.ny; i++, buffer -= reader->meta.nx) {
                if (TIFFReadScanline(geotiff16->tiff, buffer, i, 0) != 1) {
                        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                            "a libtiff error occured when reading file `%s'",
                            geotiff16->path);
                }
        }

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_reader_geotiff16_create(
    struct turtle_reader ** reader_p, struct turtle_error_context * error_)
{
        /* Allocate the geotiff16 reader */
        struct geotiff16_reader * geotiff16 = malloc(sizeof(*geotiff16));
        if (geotiff16 == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for geotiff16 reader");
        }
        *reader_p = &geotiff16->base;

        /* Initialise the reader object */
        memset(geotiff16, 0x0, sizeof(*geotiff16));
        geotiff16->tiff = NULL;
        geotiff16->path = NULL;
        geotiff16->base.meta.projection.type = PROJECTION_NONE;

        geotiff16->base.open = &geotiff16_open;
        geotiff16->base.close = &geotiff16_close;
        geotiff16->base.read = &geotiff16_read;

        geotiff16->base.meta.get_z = &get_z;
        geotiff16->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}

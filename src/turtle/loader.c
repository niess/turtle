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

#include <stdlib.h>
#include <string.h>

#include "datum.h"
#include "loader.h"
#include "turtle.h"

#ifndef TURTLE_NO_TIFF
#include "loader/geotiff16.h"

/* Copy the tile meta data. */
static void copy_geotiff_meta(
    struct geotiff16_reader * reader, struct datum_tile * tile)
{
        tile->nx = reader->width;
        tile->ny = reader->height;
        tile->x0 = reader->tiepoint[1][0];
        tile->y0 =
            reader->tiepoint[1][1] + (1. - reader->height) * reader->scale[1];
        tile->dx = reader->scale[0];
        tile->dy = reader->scale[1];
}

/* Load the geotiff meta data to a tile. */
static enum turtle_return load_geotiff_meta(
    const char * path, struct datum_tile * tile)
{
        /* Open the geotiff16 file. */
        struct geotiff16_reader reader;
        if (geotiff16_open(path, &reader) != 0) return TURTLE_RETURN_PATH_ERROR;

        /* Copy the tile meta data. */
        copy_geotiff_meta(&reader, tile);

        /* Close and return. */
        geotiff16_close(&reader);
        return TURTLE_RETURN_SUCCESS;
}

/* Load geotiff elevation data to a tile. */
static enum turtle_return load_geotiff(
    const char * path, struct datum_tile ** tile)
{
        struct geotiff16_reader reader;
        enum turtle_return rc = TURTLE_RETURN_SUCCESS;
        *tile = NULL;

        /* Open the geotiff16 file. */
        if (geotiff16_open(path, &reader) != 0) return TURTLE_RETURN_PATH_ERROR;

        /* Allocate the map. */
        *tile = malloc(
            sizeof(**tile) + sizeof(int16_t) * reader.width * reader.height);
        if (*tile == NULL) {
                rc = TURTLE_RETURN_MEMORY_ERROR;
                goto clean_and_exit;
        }

        /* Copy the tile data. */
        copy_geotiff_meta(&reader, *tile);
        if (geotiff16_readinto(&reader, *tile) != 0)
                rc = TURTLE_RETURN_BAD_FORMAT;

clean_and_exit:
        geotiff16_close(&reader);
        return rc;
}
#endif

#ifndef TURTLE_NO_HGT
#include "loader/hgt.h"

/* Copy the tile meta data. */
static void copy_hgt_meta(struct hgt_reader * reader, struct datum_tile * tile)
{
        tile->nx = reader->size;
        tile->ny = reader->size;
        tile->x0 = reader->origin[0];
        tile->y0 = reader->origin[1];
        tile->dx = reader->scale;
        tile->dy = reader->scale;
}

/* Load the hgt meta data to a tile. */
static enum turtle_return load_hgt_meta(
    const char * path, struct datum_tile * tile)
{
        /* Open the hgt file. */
        struct hgt_reader reader;
        if (hgt_open(path, &reader) != 0) return TURTLE_RETURN_PATH_ERROR;

        /* Copy the tile meta data. */
        copy_hgt_meta(&reader, tile);

        /* Close and return. */
        hgt_close(&reader);
        return TURTLE_RETURN_SUCCESS;
}

/* Load hgt elevation data to a tile. */
static enum turtle_return load_hgt(const char * path, struct datum_tile ** tile)
{
        struct hgt_reader reader;
        enum turtle_return rc = TURTLE_RETURN_SUCCESS;
        *tile = NULL;

        /* Open the hgt file. */
        if (hgt_open(path, &reader) != 0) return TURTLE_RETURN_PATH_ERROR;

        /* Allocate the map. */
        *tile = malloc(
            sizeof(**tile) + sizeof(int16_t) * reader.size * reader.size);
        if (*tile == NULL) {
                rc = TURTLE_RETURN_MEMORY_ERROR;
                goto clean_and_exit;
        }

        /* Copy the tile data. */
        copy_hgt_meta(&reader, *tile);
        if (hgt_readinto(&reader, *tile) != 0) rc = TURTLE_RETURN_BAD_FORMAT;

clean_and_exit:
        hgt_close(&reader);
        return rc;
}
#endif

/* Get the data format from the file extension */
enum loader_format loader_format(const char * path)
{
        /* Look for teh file extension */
        const char *extension = NULL, *p;
        for (p = path; *p != 0x0; p++)
                if (*p == '.') extension = p + 1;
        if (extension == NULL) return LOADER_FORMAT_UNKNOWN;

/* Check the extension */
#ifndef TURTLE_NO_TIFF
        if ((strcmp(extension, "tif") == 0) || (strcmp(extension, "TIF") == 0))
                return LOADER_FORMAT_GEOTIFF;
#endif
#ifndef TURTLE_NO_HGT
        if (strcmp(extension, "hgt") == 0) return LOADER_FORMAT_HGT;
#endif
        return LOADER_FORMAT_UNKNOWN;
}

/* Load the tile metadata */
enum turtle_return loader_meta(const char * path, struct datum_tile * tile)
{
        enum loader_format format = loader_format(path);
#ifndef TURTLE_NO_TIFF
        if (format == LOADER_FORMAT_GEOTIFF)
                return load_geotiff_meta(path, tile);
#endif
#ifndef TURTLE_NO_HGT
        if (format == LOADER_FORMAT_HGT) return load_hgt_meta(path, tile);
#endif
        return TURTLE_RETURN_BAD_FORMAT;
}

/* Load the tile data */
enum turtle_return loader_load(const char * path, struct datum_tile ** tile)
{
        enum loader_format format = loader_format(path);
#ifndef TURTLE_NO_TIFF
        if (format == LOADER_FORMAT_GEOTIFF) return load_geotiff(path, tile);
#endif
#ifndef TURTLE_NO_HGT
        if (format == LOADER_FORMAT_HGT) return load_hgt(path, tile);
#endif
        return TURTLE_RETURN_BAD_FORMAT;
}

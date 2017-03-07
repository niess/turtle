/*
 *  Topographic Utilities for Rendering The eLEvation (TURTLE)
 *  Copyright (C) 2016  Valentin Niess
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Interface to geotiff files providing a reader for 16b data, e.g. ASTER-GDEM2
 * tiles.
 */
#include "geotiff16.h"
#include <stdlib.h>
#include <string.h>

struct reader_data {
        struct geotiff16_reader api;
        TIFF * tiff;
        uint32 row;
        int16_t buffer[]; /* Placeholder for the read buffer. */
};

/* GEOTIFF tags. */
#define TIFFTAG_GEOPIXELSCALE 33550
#define TIFFTAG_INTERGRAPH_MATRIX 33920
#define TIFFTAG_GEOTIEPOINTS 33922
#define TIFFTAG_GEOKEYDIRECTORY 34735
#define TIFFTAG_GEODOUBLEPARAMS 34736
#define TIFFTAG_GEOASCIIPARAMS 34737

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
            "GeoASCIIParams" } };

/* Extender for GEOTIFF tags. */
static TIFFExtendProc parent_extender = NULL;

static void default_directory(TIFF * tiff)
{
        TIFFMergeFieldInfo(
            tiff, field_info, sizeof(field_info) / sizeof(field_info[0]));
        if (parent_extender) (*parent_extender)(tiff);
}

/* Register the tag extender to libtiff. */
void geotiff16_register()
{
        parent_extender = TIFFSetTagExtender(default_directory);
        TIFFSetErrorHandler(NULL); /* Mute error messages. */
}

int geotiff16_open(const char * path, struct geotiff16_reader * reader)
{
        /* Open the TIFF file. */
        reader->tiff = TIFFOpen(path, "r");
        if (reader->tiff == NULL) return -1;

        /* Initialise the new reader and return. */
        TIFFGetField(reader->tiff, TIFFTAG_IMAGELENGTH, &reader->width);
        tsize_t size = TIFFScanlineSize(reader->tiff);
        reader->height = size / sizeof(int16);
        int count = 0;
        double * data = NULL;
        TIFFGetField(reader->tiff, TIFFTAG_GEOPIXELSCALE, &count, &data);
        if (count == 3) memcpy(reader->scale, data, sizeof(reader->scale));
        TIFFGetField(reader->tiff, TIFFTAG_GEOTIEPOINTS, &count, &data);
        if (count == 6) {
                memcpy(reader->tiepoint[0], data, sizeof(reader->tiepoint[0]));
                memcpy(
                    reader->tiepoint[1], data + 3, sizeof(reader->tiepoint[1]));
        }

        return 0;
}

void geotiff16_close(struct geotiff16_reader * reader)
{
        if (reader == NULL) return;
        if (reader->tiff != NULL) TIFFClose(reader->tiff);
        reader->tiff = NULL;
}

int geotiff16_readinto(struct geotiff16_reader * reader, int16_t * buffer)
{
        buffer += reader->height * (reader->width - 1);
        int i;
        for (i = 0; i < reader->width; i++, buffer -= reader->height) {
                if (TIFFReadScanline(reader->tiff, buffer, i, 0) != 1)
                        return -1;
        }
        return 0;
}

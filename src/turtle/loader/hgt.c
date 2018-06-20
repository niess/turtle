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
  * Interface to hgt files providing a reader for 16b data, e.g. SRTM tiles.
  */
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "turtle.h"
#include "../datum.h"

#include "hgt.h"

int hgt_open(const char * path, struct hgt_reader * reader)
{
        /* Initialise the reader object */
        memset(reader, 0x0, sizeof(*reader));
        reader->fid = NULL;

        /* Parse the meta data from the path name */
        const char * filename = path;
        const char * p;
        for (p = path; *p != 0x0; p++) {
                if ((*p == '/') || (*p == '\\'))
                        filename = p + 1;
        }

        if (strlen(filename) < 8) return -1;
        reader->origin[0] = atoi(filename + 4);
        if (filename[3] == 'W')
                reader->origin[0] = -reader->origin[0];
        reader->origin[1] = atoi(filename + 1);
        if (filename[0] == 'S')
                reader->origin[1] = -reader->origin[1];

        for (p = filename + 8; (*p != 0x0) && (*p != '.'); p++)
                ;
        const int n = p - filename - 8;
        if ((n == 0) || (strncmp(filename + 8, "SRTMGL1", n) == 0))
                reader->size = 3601;
        else
                reader->size = 1201;
        reader->scale = 1. / (reader->size - 1);

        /* Open the file */
        reader->fid = fopen(path, "rb");
        if (reader->fid == NULL) return -1;

        return 0;
}

void hgt_close(struct hgt_reader * reader)
{
        if (reader->fid != NULL) {
                fclose(reader->fid);
                reader->fid = NULL;
        }
}

static int16_t get_z(struct datum_tile * tile, int ix, int iy)
{
        iy = tile->ny - 1 - iy;
        return ntohs(tile->data[iy * tile->nx + ix]);
}

int hgt_readinto(struct hgt_reader * reader, struct datum_tile * tile)
{
        tile->z = NULL;

        /* Load the raw data from file */
        const int n = reader->size * reader->size;
        int16_t * buffer = tile->data;
        if (fread(buffer, sizeof(*buffer), n, reader->fid) != n)
                return -1;

        /* Set the data provider */
        tile->z = &get_z;

        return 0;
}

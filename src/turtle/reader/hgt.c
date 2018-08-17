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
 * Interface to hgt files providing a reader for 16b data, e.g. SRTM tiles
 */

/* C89 standard library */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Linux library */
#include <arpa/inet.h>
/* TURTLE library */
#include "turtle/reader.h"

/* Data for reading a png file */
struct hgt_reader {
        /* Base reader object */
        struct turtle_reader base;

        /* Internal data for the reader */
        FILE * fid;
        const char * path;
};

static enum turtle_return hgt_open(struct turtle_reader * reader,
    const char * path, struct turtle_error_context * error_)
{
        struct hgt_reader * hgt = (struct hgt_reader *)reader;
        if (hgt->fid != NULL) reader->close(reader);

        /* Initialise the reader object */
        reader->meta.nx = reader->meta.ny = 0;
        reader->meta.x0 = reader->meta.y0 = 0.;
        reader->meta.dx = reader->meta.dy = reader->meta.dz = 0.;
        reader->meta.z0 = -32767.;
        reader->meta.dz = 1.;
        reader->meta.projection.type = PROJECTION_NONE;
        hgt->path = NULL;

        /* Parse the meta data from the path name */
        const char * filename = path;
        const char * p;
        for (p = path; *p != 0x0; p++) {
                if ((*p == '/') || (*p == '\\')) filename = p + 1;
        }

        const char * fmtmsg = "invalid hgt filename for `%s'";
        if (strlen(filename) < 8) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT, fmtmsg, path);
        }
        reader->meta.x0 = atoi(filename + 4);
        if (filename[3] == 'W')
                reader->meta.x0 = -reader->meta.x0;
        else if (filename[3] != 'E') {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT, fmtmsg, path);
        }
        reader->meta.y0 = atoi(filename + 1);
        if (filename[0] == 'S')
                reader->meta.y0 = -reader->meta.y0;
        else if (filename[0] != 'N') {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_BAD_FORMAT, fmtmsg, path);
        }

        const char * ext = NULL;
        for (p = filename + 7; *p != 0x0; p++) {
                if (*p == '.') ext = p + 1;
        }

        const int n = ext - filename - 8;
        if ((n == 0) || (strncmp(filename + 8, "SRTMGL1", n) == 0))
                reader->meta.nx = reader->meta.ny = 3601;
        else
                reader->meta.nx = reader->meta.ny = 1201;
        reader->meta.dx = 1. / (reader->meta.nx - 1);
        reader->meta.dy = 1. / (reader->meta.ny - 1);

        /* Open the file */
        hgt->fid = fopen(path, "rb");
        if (hgt->fid == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }

        hgt->path = path;
        return TURTLE_RETURN_SUCCESS;
}

static void hgt_close(struct turtle_reader * reader)
{
        struct hgt_reader * hgt = (struct hgt_reader *)reader;
        if (hgt->fid != NULL) {
                fclose(hgt->fid);
                hgt->fid = NULL;
                hgt->path = NULL;
        }
}

static double get_z(const struct turtle_map * map, int ix, int iy)
{
        /* TODO: check this ... */
        iy = map->meta.ny - 1 - iy;
        return (int16_t)ntohs(map->data[iy * map->meta.nx + ix]);
}

static void set_z(struct turtle_map * map, int ix, int iy, double z)
{
        iy = map->meta.ny - 1 - iy;
        map->data[iy * map->meta.nx + ix] = (uint16_t)(int16_t)htons(z);
}

static enum turtle_return hgt_read(struct turtle_reader * reader,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct hgt_reader * hgt = (struct hgt_reader *)reader;

        /* Load the raw data from file */
        const int n = reader->meta.nx * reader->meta.ny;
        if (fread(map->data, sizeof(*map->data), n, hgt->fid) != n) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "missing data when reading file `%s'", hgt->path);
        } else
                return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_reader_hgt_create(
    struct turtle_reader ** reader_p, struct turtle_error_context * error_)
{
        /* Allocate the hgt reader */
        struct hgt_reader * hgt = malloc(sizeof(*hgt));
        if (hgt == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for hgt reader");
        }
        *reader_p = &hgt->base;

        /* Initialise the reader object */
        memset(hgt, 0x0, sizeof(*hgt));
        hgt->fid = NULL;
        hgt->path = NULL;
        hgt->base.meta.projection.type = PROJECTION_NONE;

        hgt->base.open = &hgt_open;
        hgt->base.close = &hgt_close;
        hgt->base.read = &hgt_read;

        hgt->base.meta.get_z = &get_z;
        hgt->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}

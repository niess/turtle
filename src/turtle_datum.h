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
 * Turtle datum handle for access to world-wide elevation data.
 */
#ifndef TURTLE_DATUM_H
#define TURTLE_DATUM_H

#include <stdint.h>

enum datum_format {
        DATUM_FORMAT_NONE = -1,
        DATUM_FORMAT_GEOTIFF,
        N_DATUM_FORMATS
};

/* Container for a data tile. */
struct datum_tile {
        /* Meta data */
        struct datum_tile *prev, *next;
        int clients;

        /* Map data. */
        int nx, ny;
        double x0, y0;
        double dx, dy;
        int16_t z[];
};

/* Container for a geodetic datum. */
struct turtle_datum {
        /* The stack of loaded tiles. */
        struct datum_tile * stack;
        int stack_size, max_size;

        /* Data format. */
        enum datum_format format;

        /* Callbacks for lock handling. */
        turtle_datum_cb * lock;
        turtle_datum_cb * unlock;

        /* Lookup data for tile's file names. */
        double latitude_0, latitude_delta;
        double longitude_0, longitude_delta;
        int latitude_n, longitude_n;
        char ** path;

        char data[]; /* Placeholder for data. */
};

/* Tile utility routines. */
void datum_tile_touch(struct turtle_datum * datum, struct datum_tile * tile);
void datum_tile_destroy(struct turtle_datum * datum, struct datum_tile * tile);
enum turtle_return datum_tile_load(
    struct turtle_datum * datum, int latitude, int longitude);

#endif

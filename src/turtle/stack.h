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
 * Turtle handle for accessing world-wide elevation data.
 */
#ifndef TURTLE_STACK_H
#define TURTLE_STACK_H

#include "turtle.h"

#include <stdint.h>

/* Callback for accessing elevation data */
struct tile;
typedef int16_t tile_cb(struct tile * tile, int ix, int iy);

/* Container for a data tile. */
struct tile {
        /* Meta data */
        struct tile *prev, *next;
        int clients;

        /* Callback for accessing elevation data */
        tile_cb * z;

        /* Map data. */
        int nx, ny;
        double x0, y0;
        double dx, dy;
        int16_t data[];
};

/* Container for a stack of global topography data. */
struct turtle_stack {
        /* The stack of loaded tiles. */
        struct tile * head;
        int size, max_size;

        /* Callbacks for lock handling. */
        turtle_stack_cb * lock;
        turtle_stack_cb * unlock;

        /* Lookup data for tile's file names. */
        double latitude_0, latitude_delta;
        double longitude_0, longitude_delta;
        int latitude_n, longitude_n;
        char * root;
        char ** path;

        char data[]; /* Placeholder for data. */
};

/* Tile utility routines. */
void tile_touch(struct turtle_stack * stack, struct tile * tile);
void tile_destroy(struct turtle_stack * stack, struct tile * tile);
enum turtle_return tile_load(
    struct turtle_stack * stack, double latitude, double longitude);

#endif

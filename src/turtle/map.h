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
 * Turtle projection map handle for managing local maps.
 */
#ifndef TURTLE_MAP_H
#define TURTLE_MAP_H

/* C89 standard library */
#include <stdint.h>
/* Turtle library */
#include "turtle/list.h"
#include "turtle/projection.h"

/* Callbacks for getting and setting elevation data */
struct turtle_map;
typedef double turtle_map_getter_t(
    const struct turtle_map * map, int ix, int iy);
typedef void turtle_map_setter_t(
    struct turtle_map * map, int ix, int iy, double z);

/* Header container for map meta data */
struct turtle_map_meta {
        /* Map meta data */
        int nx, ny;
        double x0, y0, z0;
        double dx, dy, dz;

        /* Callbacks for getting and setting elevation data */
        turtle_map_getter_t * get_z;
        turtle_map_setter_t * set_z;

        /* Data encoding format */
        char encoding[8];

        /* Projection */
        struct turtle_projection projection;
};

/* Container for a map */
struct turtle_map {
        struct turtle_list_element element;

        /* Meta data */
        struct turtle_map_meta meta;

        /* Stack data */
        struct turtle_stack * stack;
        int clients;

        /* Placeholder for raw elevation data */
        uint16_t data[];
};

enum turtle_return turtle_map_elevation_(const struct turtle_map * map,
    double x, double y, double * z, int * inside,
    struct turtle_error_context * error_);

enum turtle_return turtle_map_load_(struct turtle_map ** map, const char * path,
    struct turtle_error_context * error_);

#endif

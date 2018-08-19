/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRacking The eLEvation (TURTLE)
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
 * Turtle handle for accessing world-wide elevation data
 */
#ifndef TURTLE_STACK_H
#define TURTLE_STACK_H

#include "turtle.h"
#include "turtle/map.h"

/* Container for a stack of global topography data */
struct turtle_stack {
        /* The stack of loaded tiles */
        struct turtle_map * head;
        int size, max_size;

        /* Callbacks for managing concurent accesses to the stack */
        turtle_stack_locker_t * lock;
        turtle_stack_locker_t * unlock;

        /* Lookup data for tile's file names */
        double latitude_0, latitude_delta;
        double longitude_0, longitude_delta;
        int latitude_n, longitude_n;
        char * root;
        char ** path;

        char data[]; /* Placeholder for data */
};

/* Map management routines */
void turtle_stack_touch_(struct turtle_stack * stack, struct turtle_map * map);
struct turtle_error_context;
enum turtle_return turtle_stack_load_(struct turtle_stack * stack,
    double latitude, double longitude, int * inside,
    struct turtle_error_context * error_);

#endif

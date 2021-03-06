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
 * Turtle client for multithreaded access to an elevation data handled by a
 * turtle_stack.
 */
#ifndef TURTLE_CLIENT_H
#define TURTLE_CLIENT_H

#include "turtle.h"
#include "turtle/map.h"

/* Container for a stack client */
struct turtle_client {
        /* The currently used map */
        struct turtle_map * map;

        /* The last requested indices */
        int index_la, index_lo;

        /* The master stack */
        struct turtle_stack * stack;
};

struct turtle_error_context;
enum turtle_return turtle_client_destroy_(
    struct turtle_client ** client, struct turtle_error_context * error_);

#endif

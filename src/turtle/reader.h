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

/* Generic map reader for the TURTLE library */
#ifndef TURTLE_READER_H
#define TURTLE_READER_H

/* TURTLE library */
#include "turtle.h"
#include "turtle/error.h"
#include "turtle/map.h"

struct turtle_reader;
typedef enum turtle_return turtle_reader_opener_t(struct turtle_reader * reader,
    const char * path, struct turtle_error_context * error_);
typedef void turtle_reader_closer_t(struct turtle_reader * reader);
typedef enum turtle_return turtle_reader_t(struct turtle_reader * reader,
    struct turtle_map * map, struct turtle_error_context * error_);

struct turtle_reader {
        /* Meta data for the map */
        struct turtle_map_meta meta;

        /* Generic reader methods */
        turtle_reader_opener_t * open;
        turtle_reader_closer_t * close;
        turtle_reader_t * read;
};

/* Generic reader allocator, given a file name */
enum turtle_return turtle_reader_create(struct turtle_reader ** reader,
    const char * path, struct turtle_error_context * error_);

#endif

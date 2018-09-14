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

/* Generic read/write for the TURTLE library */
#ifndef TURTLE_IO_H
#define TURTLE_IO_H

/* TURTLE library */
#include "turtle.h"
#include "turtle/error.h"
#include "turtle/map.h"

struct turtle_io;
typedef enum turtle_return turtle_io_opener_t(struct turtle_io * io,
    const char * path, const char * mode, struct turtle_error_context * error_);
typedef void turtle_io_closer_t(struct turtle_io * io);
typedef enum turtle_return turtle_io_reader_t(struct turtle_io * io,
    struct turtle_map * map, struct turtle_error_context * error_);
typedef enum turtle_return turtle_io_writer_t(struct turtle_io * io,
    const struct turtle_map * map, struct turtle_error_context * error_);

struct turtle_io {
        /* Meta data for the map */
        struct turtle_map_meta meta;

        /* Generic io methods */
        turtle_io_opener_t * open;
        turtle_io_closer_t * close;
        turtle_io_reader_t * read;
        turtle_io_writer_t * write;
};

/* Generic io allocator, given a file name */
enum turtle_return turtle_io_create_(struct turtle_io ** io, const char * path,
    struct turtle_error_context * error_);

#endif

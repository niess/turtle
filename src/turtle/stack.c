/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicUles over Long rangEs (TURTLE)
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

/* C89 standard library */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* TinyDir library */
#include "deps/tinydir.h"
/* TURTLE library */
#include "turtle.h"
#include "turtle/error.h"
#include "turtle/io.h"
#include "turtle/stack.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* Create a new stack of maps */
enum turtle_return turtle_stack_create(struct turtle_stack ** stack,
    const char * path, int size, turtle_stack_locker_t * lock,
    turtle_stack_locker_t * unlock)
{
        TURTLE_ERROR_INITIALISE(&turtle_stack_create);
        *stack = NULL;

        /* Check the lock and unlock consistency. */
        if (((lock == NULL) && (unlock != NULL)) ||
            ((unlock == NULL) && (lock != NULL)))
                return TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_BAD_ADDRESS, "inconsistent lock & unlock");

        /* Scan the provided path for the map data */
        double lat_min = DBL_MAX, long_min = DBL_MAX;
        double lat_max = -DBL_MAX, long_max = -DBL_MAX;
        double lat_delta = 0., long_delta = 0.;
        int data_size = strlen(path) + 1;

        int rc;
        tinydir_dir dir;
        for (rc = tinydir_open(&dir, path); (rc == 0) && dir.has_next;
             tinydir_next(&dir)) {
                tinydir_file file;
                tinydir_readfile(&dir, &file);
                if (file.is_dir) continue;

                /* Get the map meta-data */
                enum turtle_return trc;
                struct turtle_io * io;
                if ((trc = turtle_io_create_(&io, file.path, error_)) ==
                    TURTLE_RETURN_BAD_EXTENSION) {
                        error_->code = TURTLE_RETURN_SUCCESS;
                        continue;
                } else if (trc != TURTLE_RETURN_SUCCESS)
                        goto error;
                if (io->open(io, file.path, "rb", error_) !=
                    TURTLE_RETURN_SUCCESS)
                        goto error;

                /* Update the lookup data */
                const double dx = io->meta.dx * (io->meta.nx - 1);
                const double dy = io->meta.dy * (io->meta.ny - 1);
                if (long_delta == 0.)
                        long_delta = dx;
                else if (long_delta != dx) {
                        TURTLE_ERROR_REGISTER(TURTLE_RETURN_BAD_FORMAT,
                            "inconsistent longitude span");
                        goto error;
                }
                if (lat_delta == 0.)
                        lat_delta = dy;
                else if (lat_delta != dy) {
                        TURTLE_ERROR_REGISTER(TURTLE_RETURN_BAD_FORMAT,
                            "inconsistent latitude span");
                        goto error;
                }
                if (io->meta.x0 < long_min) long_min = io->meta.x0;
                if (io->meta.y0 < lat_min) lat_min = io->meta.y0;
                const double x1 = io->meta.x0 + dx;
                if (x1 > long_max) long_max = x1;
                const double y1 = io->meta.y0 + dy;
                if (y1 > lat_max) lat_max = y1;
                data_size += strlen(file.path) + 1;

                io->close(io);
                free(io);
                continue;
        error:
                if (io != NULL) {
                        io->close(io);
                        free(io);
                }
                tinydir_close(&dir);
                return TURTLE_ERROR_RAISE();
        }
        if (rc == 0) tinydir_close(&dir);

        /* Check the grid size */
        int lat_n = 0, long_n = 0;
        if ((lat_delta > 0.) && (long_delta > 0.)) {
                const double dx = (long_max - long_min) / long_delta;
                long_n = (int)(dx + FLT_EPSILON);
                if (fabs(long_n - dx) > FLT_EPSILON)
                        return TURTLE_ERROR_MESSAGE(
                            TURTLE_RETURN_BAD_FORMAT, "invalid longitude grid");
                const double dy = (lat_max - lat_min) / lat_delta;
                lat_n = (int)(dy + FLT_EPSILON);
                if (fabs(lat_n - dy) > FLT_EPSILON)
                        return TURTLE_ERROR_MESSAGE(
                            TURTLE_RETURN_BAD_FORMAT, "invalid latitude grid");
        }

        /* Allocate the new stack handle */
        const int path_size = lat_n * long_n * sizeof(char *);
        data_size += path_size;
        *stack = malloc(sizeof(**stack) + data_size);
        if (*stack == NULL) return TURTLE_ERROR_MEMORY();

        /* Initialise the handle */
        (*stack)->head = NULL;
        (*stack)->size = 0;
        (*stack)->max_size = (size > 0) ? size : INT_MAX;
        (*stack)->lock = lock;
        (*stack)->unlock = unlock;
        (*stack)->latitude_0 = lat_min;
        (*stack)->longitude_0 = long_min;
        (*stack)->latitude_delta = lat_delta;
        (*stack)->longitude_delta = long_delta;
        (*stack)->latitude_n = lat_n;
        (*stack)->longitude_n = long_n;
        const int root_size = strlen(path) + 1;
        (*stack)->root = (char *)((*stack)->data);
        memcpy((*stack)->root, path, root_size);
        (*stack)->path = (char **)((*stack)->data + root_size);

        if ((lat_n == 0) || (long_n == 0)) return TURTLE_RETURN_SUCCESS;

        /* Build the lookup data */
        int i;
        for (i = 0; i < lat_n * long_n; i++) (*stack)->path[i] = NULL;

        char * cursor = (char *)((*stack)->path) + path_size;
        for (tinydir_open(&dir, path); dir.has_next; tinydir_next(&dir)) {
                tinydir_file file;
                tinydir_readfile(&dir, &file);
                if (file.is_dir) continue;

                /* Check the format. */
                struct turtle_io * io;
                if (turtle_io_create_(&io, file.path, error_) !=
                    TURTLE_RETURN_SUCCESS) {
                        error_->code = TURTLE_RETURN_SUCCESS;
                        continue;
                }
                io->open(io, file.path, "rb", error_);

                /* Compute the lookup index */
                const int ix = (int)((io->meta.x0 - long_min) / long_delta);
                const int iy = (int)((io->meta.y0 - lat_min) / lat_delta);
                i = iy * long_n + ix;
                io->close(io);
                free(io);

                /* Copy the path name */
                const int n = strlen(file.path) + 1;
                memcpy(cursor, file.path, n);
                (*stack)->path[i] = cursor;
                cursor += n;
        }
        tinydir_close(&dir);

        return TURTLE_RETURN_SUCCESS;
}

/* Low level routine for cleaning the stack */
static void stack_clear(struct turtle_stack * stack, int force)
{
        struct turtle_map * map = stack->head;
        while (map != NULL) {
                struct turtle_map * current = map;
                map = map->prev;
                if ((force != 0) || (current->clients == 0))
                        turtle_map_destroy(&current);
        }
}

/* Destroy a stack and all its loaded maps */
void turtle_stack_destroy(struct turtle_stack ** stack)
{
        if ((stack == NULL) || (*stack == NULL)) return;

        /* Force the stack cleaning */
        stack_clear(*stack, 1);

        /* Delete the stack and return */
        free(*stack);
        *stack = NULL;
}

/* Clear the stack from unused maps */
enum turtle_return turtle_stack_clear(struct turtle_stack * stack)
{
        TURTLE_ERROR_INITIALISE(&turtle_stack_clear);
        if ((stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_ERROR_LOCK();

        /* Soft clean of the stack */
        stack_clear(stack, 0);

        if ((stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_ERROR_UNLOCK();
        else
                return TURTLE_RETURN_SUCCESS;
}

/* Load the stack elevation data into memory */
enum turtle_return turtle_stack_load(struct turtle_stack * stack)
{
        TURTLE_ERROR_INITIALISE(&turtle_stack_load);
        if ((stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_ERROR_LOCK();

        double x = stack->longitude_0 + 0.5 * stack->longitude_delta;
        double y = stack->latitude_0 + 0.5 * stack->latitude_delta;
        struct turtle_map * head = stack->head;
        while (stack->size < stack->max_size) {
                /* First, let us check the initial stack */
                int load = 1;
                struct turtle_map * map = head;
                while (map != NULL) {
                        const double hx = (x - map->meta.x0) / map->meta.dx;
                        const double hy = (y - map->meta.y0) / map->meta.dy;

                        if ((hx >= 0.) && (hx <= map->meta.nx) && (hy >= 0.) &&
                            (hy <= map->meta.ny)) {
                                load = 0;
                                break;
                        }
                        map = map->prev;
                }

                if (load) {
                        int inside;
                        if (turtle_stack_load_(stack, y, x, &inside, error_) !=
                            TURTLE_RETURN_SUCCESS)
                                break;
                }

                /* Update the coordinates and check for termination */
                x += stack->longitude_delta;
                if (x > stack->longitude_0 +
                        stack->longitude_n * stack->longitude_delta) {
                        y += stack->latitude_delta;
                        if (y > stack->latitude_0 +
                                stack->latitude_n * stack->latitude_delta)
                                break;
                        x = stack->longitude_0 + 0.5 * stack->longitude_delta;
                }
        }

        if ((stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_ERROR_UNLOCK();
        else
                return TURTLE_ERROR_RAISE();
}

/* Get the elevation at the given geodetic coordinates */
enum turtle_return turtle_stack_elevation(struct turtle_stack * stack,
    double latitude, double longitude, double * elevation, int * inside)
{
        TURTLE_ERROR_INITIALISE(&turtle_stack_elevation);
        if (inside != NULL) *inside = 0;

        /* Get the proper map */
        double hx, hy;
        int load = 1;
        if (stack->head != NULL) {
                /* First let's check the top of the stack */
                struct turtle_map * head = stack->head;
                hx = (longitude - head->meta.x0) / head->meta.dx;
                hy = (latitude - head->meta.y0) / head->meta.dy;

                if ((hx < 0.) || (hx >= head->meta.nx - 1) || (hy < 0.) ||
                    (hy >= head->meta.ny - 1)) {
                        /* The requested coordinates are not in the top
                         * maps. Let's check the full stack */
                        struct turtle_map * map = head->prev;
                        while (map != NULL) {
                                hx = (longitude - map->meta.x0) / map->meta.dx;
                                hy = (latitude - map->meta.y0) / map->meta.dy;

                                if ((hx >= 0.) && (hx < map->meta.nx - 1) &&
                                    (hy >= 0.) && (hy < map->meta.ny - 1)) {
                                        /*
                                         * Move the valid map to the top
                                         * of the stack
                                         */
                                        turtle_stack_touch_(stack, map);
                                        load = 0;
                                        break;
                                }
                                map = map->prev;
                        }
                } else
                        load = 0;
        }

        if (load) {
                /* No valid map was found. Let's try to load it */
                enum turtle_return rc = turtle_stack_load_(
                    stack, latitude, longitude, inside, error_);
                if ((rc != TURTLE_RETURN_SUCCESS) ||
                    ((inside != NULL) && (*inside == 0))) {
                        *elevation = 0.;
                        return TURTLE_ERROR_RAISE();
                }

                struct turtle_map * head = stack->head;
                hx = (longitude - head->meta.x0) / head->meta.dx;
                hy = (latitude - head->meta.y0) / head->meta.dy;
        }

        /* Interpolate the elevation */
        return turtle_map_elevation_(
            stack->head, longitude, latitude, elevation, inside, error_);
}

/* Move a map to the top of the stack */
void turtle_stack_touch_(struct turtle_stack * stack, struct turtle_map * map)
{
        if (map->next == NULL) return; /* Already on top */
        struct turtle_map * prev = map->prev;
        map->next->prev = prev;
        if (prev != NULL) prev->next = map->next;
        stack->head->next = map;
        map->prev = stack->head;
        map->next = NULL;
        stack->head = map;
}

/* Load a new map and manage the stack */
enum turtle_return turtle_stack_load_(struct turtle_stack * stack,
    double latitude, double longitude, int * inside,
    struct turtle_error_context * error_)
{
#define RETURN_OR_RAISE()                                                      \
        {                                                                      \
                if (inside != NULL) {                                          \
                        return TURTLE_RETURN_SUCCESS;                          \
                } else {                                                       \
                        return TURTLE_ERROR_MISSING_DATA(stack);               \
                }                                                              \
        }

        /* Lookup the requested file */
        if (inside != NULL) *inside = 0;
        if ((longitude < stack->longitude_0) || (latitude < stack->latitude_0))
                RETURN_OR_RAISE()
        const int ix =
            (int)((longitude - stack->longitude_0) / stack->longitude_delta);
        if (ix >= stack->longitude_n) RETURN_OR_RAISE()
        const int iy =
            (int)((latitude - stack->latitude_0) / stack->latitude_delta);
        if (iy >= stack->latitude_n) RETURN_OR_RAISE()
        const int index = iy * stack->longitude_n + ix;
        if (stack->path[index] == NULL) RETURN_OR_RAISE()
#undef RETURN_OR_RAISE

        /* Load the map data according to the format */
        struct turtle_map * map;
        if (turtle_map_load_(&map, stack->path[index], error_) !=
            TURTLE_RETURN_SUCCESS)
                return error_->code;

        /* Make room for the new map, if needed */
        if (stack->size >= stack->max_size) {
                struct turtle_map * last = stack->head;
                while (last->prev != NULL) last = last->prev;
                while ((last != NULL) && (stack->size >= stack->max_size)) {
                        struct turtle_map * current = last;
                        last = last->next;
                        if (current->clients == 0) turtle_map_destroy(&current);
                }
        }

        /* Append the new map on top of the stack */
        map->stack = stack;
        map->prev = stack->head;
        if (stack->head != NULL) stack->head->next = map;
        stack->head = map;
        stack->size++;

        if (inside != NULL) *inside = 1;
        return TURTLE_RETURN_SUCCESS;
}

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
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tinydir.h"

#include "stack.h"
#include "error.h"
#include "loader.h"
#include "turtle.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* Create a new tiles stack. */
enum turtle_return turtle_stack_create(const char * path, int size,
    turtle_stack_cb * lock, turtle_stack_cb * unlock,
    struct turtle_stack ** stack)
{
        *stack = NULL;

        /* Check the lock and unlock consistency. */
        if (((lock == NULL) && (unlock != NULL)) ||
            ((unlock == NULL) && (lock != NULL)))
                return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_ADDRESS,
                    turtle_stack_create, "inconsistent lock & unlock");

        /* Scan the provided path for the tile data. */
        double lat_min = 1E+05, long_min = 1E+05;
        double lat_max = -1E+05, long_max = -1E+05;
        double lat_delta = 0., long_delta = 0.;
        int data_size = strlen(path) + 1;

        int rc;
        tinydir_dir dir;
        for (rc = tinydir_open(&dir, path); (rc == 0) && dir.has_next;
             tinydir_next(&dir)) {
                tinydir_file file;
                tinydir_readfile(&dir, &file);
                if (file.is_dir) continue;

                /* Check the format. */
                if (loader_format(file.path) == LOADER_FORMAT_UNKNOWN) continue;

                /* Get the tile meta-data. */
                struct tile tile;
                if (loader_meta(file.path, &tile) != TURTLE_RETURN_SUCCESS)
                        continue;

                /* Update the lookup data. */
                const double dx = tile.dx * (tile.nx - 1);
                const double dy = tile.dy * (tile.ny - 1);
                if (long_delta == 0.)
                        long_delta = dx;
                else if (long_delta != dx)
                        goto format_error;
                if (lat_delta == 0.)
                        lat_delta = dy;
                else if (lat_delta != dy)
                        goto format_error;
                if (tile.x0 < long_min) long_min = tile.x0;
                if (tile.y0 < lat_min) lat_min = tile.y0;
                const double x1 = tile.x0 + dx;
                if (x1 > long_max) long_max = x1;
                const double y1 = tile.y0 + dy;
                if (y1 > lat_max) lat_max = y1;
                data_size += strlen(file.path) + 1;
        }
        if (rc == 0) tinydir_close(&dir);

        /* Check the grid size. */
        int lat_n = 0, long_n = 0;
        if ((lat_delta > 0.) && (long_delta > 0.)) {
                const double dx = (long_max - long_min) / long_delta;
                long_n = (int)(dx + FLT_EPSILON);
                if (fabs(long_n - dx) > FLT_EPSILON)
                        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_FORMAT,
                            turtle_stack_create, "invalid longitude grid");
                const double dy = (lat_max - lat_min) / lat_delta;
                lat_n = (int)(dy + FLT_EPSILON);
                if (fabs(lat_n - dy) > FLT_EPSILON)
                        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_FORMAT,
                            turtle_stack_create, "invalid latitude grid");
        }

        /* Allocate the new stack handle. */
        const int path_size = lat_n * long_n * sizeof(char *);
        data_size += path_size;
        *stack = malloc(sizeof(**stack) + data_size);
        if (*stack == NULL) return TURTLE_ERROR_MEMORY(turtle_stack_create);

        /* Initialise the handle. */
        (*stack)->head = NULL;
        (*stack)->size = 0;
        (*stack)->max_size = size;
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

        /* Build the lookup data. */
        int i;
        for (i = 0; i < lat_n * long_n; i++) (*stack)->path[i] = NULL;

        char * cursor = (char *)((*stack)->path) + path_size;
        for (tinydir_open(&dir, path); dir.has_next; tinydir_next(&dir)) {
                tinydir_file file;
                tinydir_readfile(&dir, &file);
                if (file.is_dir) continue;

                /* Check the format. */
                if (loader_format(file.path) == LOADER_FORMAT_UNKNOWN) continue;

                /* Check the tile meta-data. */
                struct tile tile;
                if (loader_meta(file.path, &tile) != TURTLE_RETURN_SUCCESS)
                        continue;

                /* Compute the lookup index. */
                const int ix = (int)((tile.x0 - long_min) / long_delta);
                const int iy = (int)((tile.y0 - lat_min) / lat_delta);
                i = iy * long_n + ix;

                /* Copy the path name. */
                const int n = strlen(file.path) + 1;
                memcpy(cursor, file.path, n);
                (*stack)->path[i] = cursor;
                cursor += n;
        }
        tinydir_close(&dir);

        return TURTLE_RETURN_SUCCESS;

format_error:
        tinydir_close(&dir);
        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_FORMAT,
            turtle_stack_create, "inconsistent elevation tiles");
}

/* Low level routine for cleaning the stack. */
void stack_clear(struct turtle_stack * stack, int force)
{
        struct tile * tile = stack->head;
        while (tile != NULL) {
                struct tile * current = tile;
                tile = tile->prev;
                if ((force != 0) || (current->clients == 0))
                        tile_destroy(stack, current);
        }
}

/* Destroy a stack and all its loaded tiles. */
void turtle_stack_destroy(struct turtle_stack ** stack)
{
        if ((stack == NULL) || (*stack == NULL)) return;

        /* Force the stack cleaning. */
        stack_clear(*stack, 1);

        /* Delete the stack and return. */
        free(*stack);
        *stack = NULL;
}

/* Clear the stack from unused tiles. */
enum turtle_return turtle_stack_clear(struct turtle_stack * stack)
{
        if ((stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_ERROR_LOCK(turtle_stack_clear);

        /* Soft clean of the stack. */
        stack_clear(stack, 0);

        if ((stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_ERROR_UNLOCK(turtle_stack_clear);
        else
                return TURTLE_RETURN_SUCCESS;
}

/* Get the elevation at the given geodetic coordinates. */
enum turtle_return turtle_stack_elevation(struct turtle_stack * stack,
    double latitude, double longitude, double * elevation, int * inside)
{
        if (inside != NULL) *inside = 0;

        /* Get the proper tile. */
        double hx, hy;
        int load = 1;
        if (stack->head != NULL) {
                /* First let's check the top of the stack. */
                struct tile * head = stack->head;
                hx = (longitude - head->x0) / head->dx;
                hy = (latitude - head->y0) / head->dy;

                if ((hx < 0.) || (hx > head->nx) || (hy < 0.) ||
                    (hy > head->ny)) {
                        /* The requested coordinates are not in the top
                         * tile. Let's check the full stack. */
                        struct tile * tile = head->prev;
                        while (tile != NULL) {
                                hx = (longitude - tile->x0) / tile->dx;
                                hy = (latitude - tile->y0) / tile->dy;

                                if ((hx >= 0.) && (hx <= tile->nx) &&
                                    (hy >= 0.) && (hy <= tile->ny)) {
                                        /*
                                         * Move the valid tile to the top
                                         * of the stack.
                                         */
                                        tile_touch(stack, tile);
                                        load = 0;
                                        break;
                                }
                                tile = tile->prev;
                        }
                } else
                        load = 0;
        }

        if (load) {
                /* No valid tile was found. Let's try to load it. */
                enum turtle_return rc =
                    tile_load(stack, latitude, longitude);
                if (rc == TURTLE_RETURN_MEMORY_ERROR) {
                        return TURTLE_ERROR_MEMORY(turtle_stack_elevation);
                } else if (rc == TURTLE_RETURN_PATH_ERROR) {
                        if (inside != NULL) {
                                return TURTLE_RETURN_SUCCESS;
                        } else {
                                return TURTLE_ERROR_MISSING_DATA(
                                    turtle_stack_elevation, stack);
                        }
                } else if (rc != TURTLE_RETURN_SUCCESS) {
                        return TURTLE_ERROR_UNEXPECTED(
                            rc, turtle_stack_elevation);
                }

                struct tile * head = stack->head;
                hx = (longitude - head->x0) / head->dx;
                hy = (latitude - head->y0) / head->dy;
        }

        /* Interpolate the elevation. */
        int ix = (int)hx;
        int iy = (int)hy;
        hx -= ix;
        hy -= iy;
        const int nx = stack->head->nx;
        const int ny = stack->head->ny;
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        int ix1 = (ix >= nx - 1) ? nx - 1 : ix + 1;
        int iy1 = (iy >= ny - 1) ? ny - 1 : iy + 1;
        struct tile * tile = stack->head;
        tile_cb * zm = tile->z;
        *elevation = zm(tile, ix, iy) * (1. - hx) * (1. - hy) +
            zm(tile, ix, iy1) * (1. - hx) * hy +
            zm(tile, ix1, iy) * hx * (1. - hy) + zm(tile, ix1, iy1) * hx * hy;

        if (inside != NULL) *inside = 1;
        return TURTLE_RETURN_SUCCESS;
}

/* Move a tile to the top of the stack. */
void tile_touch(struct turtle_stack * stack, struct tile * tile)
{
        if (tile->next == NULL) return; /* Already on top. */
        struct tile * prev = tile->prev;
        tile->next->prev = prev;
        if (prev != NULL) prev->next = tile->next;
        stack->head->next = tile;
        tile->prev = stack->head;
        tile->next = NULL;
        stack->head = tile;
}

/* Remove a tile from the stack. */
void tile_destroy(struct turtle_stack * stack, struct tile * tile)
{
        struct tile * prev = tile->prev;
        struct tile * next = tile->next;
        if (prev != NULL) prev->next = next;
        if (next != NULL)
                next->prev = prev;
        else
                stack->head = prev;
        free(tile);
        stack->size--;
}

/* Load a new tile and manage the stack. */
enum turtle_return tile_load(
    struct turtle_stack * stack, double latitude, double longitude)
{
        /* Lookup the requested file. */
        if ((longitude < stack->longitude_0) || (latitude < stack->latitude_0))
                return TURTLE_RETURN_PATH_ERROR;
        const int ix =
            (int)((longitude - stack->longitude_0) / stack->longitude_delta);
        if (ix >= stack->longitude_n) return TURTLE_RETURN_PATH_ERROR;
        const int iy =
            (int)((latitude - stack->latitude_0) / stack->latitude_delta);
        if (iy >= stack->latitude_n) return TURTLE_RETURN_PATH_ERROR;
        const int index = iy * stack->longitude_n + ix;
        if (stack->path[index] == NULL) return TURTLE_RETURN_PATH_ERROR;

        /* Load the tile data according to the format. */
        struct tile * tile = NULL;
        enum turtle_return rc;
        if ((rc = loader_load(stack->path[index], &tile)) !=
            TURTLE_RETURN_SUCCESS)
                return rc;

        /* Initialise the client's references. */
        tile->clients = 0;

        /* Make room for the new tile, if needed. */
        if (stack->size >= stack->max_size) {
                struct tile * last = stack->head;
                while (last->prev != NULL) last = last->prev;
                while (
                    (last != NULL) && (stack->size >= stack->max_size)) {
                        struct tile * current = last;
                        last = last->next;
                        if (current->clients == 0)
                                tile_destroy(stack, current);
                }
        }

        /* Append the new tile on top of the stack. */
        tile->next = NULL;
        tile->prev = stack->head;
        if (stack->head != NULL) stack->head->next = tile;
        stack->head = tile;
        stack->size++;

        return TURTLE_RETURN_SUCCESS;
}

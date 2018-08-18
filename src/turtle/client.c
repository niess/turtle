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
 * Turtle client for multithreaded access to a elevation data handled by a
 * turtle_stack.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "error.h"
#include "stack.h"
#include "turtle.h"

/* Management routine(s) */
static enum turtle_return client_release(struct turtle_client * client,
    int lock, struct turtle_error_context * error_);

/* Create a new stack client */
enum turtle_return turtle_client_create(
    struct turtle_client ** client, struct turtle_stack * stack)
{
        TURTLE_ERROR_INITIALISE(&turtle_client_create);

        /* Check that one has a valid stack */
        *client = NULL;
        if ((stack == NULL) || (stack->lock == NULL))
                TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_BAD_ADDRESS, "invalid stack or missing lock");

        /* Allocate the new client and initialise it. */
        *client = malloc(sizeof(**client));
        if (*client == NULL) return TURTLE_ERROR_MEMORY();
        (*client)->stack = stack;
        (*client)->map = NULL;
        (*client)->index_la = INT_MIN;
        (*client)->index_lo = INT_MIN;

        return TURTLE_RETURN_SUCCESS;
}

/* Destroy a client */
enum turtle_return turtle_client_destroy_(
    struct turtle_client ** client, struct turtle_error_context * error_)
{
        /* Check if the client has already been released. */
        if ((client == NULL) || (*client == NULL)) return TURTLE_RETURN_SUCCESS;

        /* Release any active map */
        if (client_release(*client, 1, error_) != TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();

        /* Free the memory and return */
        free(*client);
        *client = NULL;

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_client_destroy(struct turtle_client ** client)
{
        TURTLE_ERROR_INITIALISE(&turtle_client_destroy);
        return turtle_client_destroy_(client, error_);
}

/* Clear the client's memory */
enum turtle_return turtle_client_clear(struct turtle_client * client)
{
        TURTLE_ERROR_INITIALISE(&turtle_client_clear);
        client_release(client, 1, error_);
        return TURTLE_ERROR_RAISE();
}

/* Supervised access to the elevation data */
enum turtle_return turtle_client_elevation(struct turtle_client * client,
    double latitude, double longitude, double * elevation, int * inside)
{
        TURTLE_ERROR_INITIALISE(&turtle_client_elevation);
        if (inside != NULL) *inside = 0;

        /* Get the proper map */
        struct turtle_map * current = client->map;
        double hx = 0., hy = 0.; /* Patch a non relevant warning */

        if (current != NULL) {
                /* First let's check the current map */
                hx = (longitude - current->meta.x0) / current->meta.dx;
                hy = (latitude - current->meta.y0) / current->meta.dy;

                if ((hx >= 0.) && (hx < current->meta.nx - 1) && (hy >= 0.) &&
                    (hy < current->meta.ny - 1))
                        goto interpolate;
        } else if (((int)latitude == client->index_la) &&
            ((int)longitude == client->index_lo)) {
                if (inside != NULL) {
                        return TURTLE_RETURN_SUCCESS;
                } else {
                        return TURTLE_ERROR_MISSING_DATA(client->stack);
                }
        }

        /* Lock the stack */
        struct turtle_stack * stack = client->stack;
        if ((stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_ERROR_LOCK();

        /* The requested coordinates are not in the current map. Let's check
         * the full stack
         */
        current = stack->head;
        while (current != NULL) {
                if (current == client->map) {
                        current = current->prev;
                        continue;
                }
                hx = (longitude - current->meta.x0) / current->meta.dx;
                hy = (latitude - current->meta.y0) / current->meta.dy;
                if ((hx >= 0.) && (hx < current->meta.nx - 1) && (hy >= 0.) &&
                    (hy < current->meta.ny - 1)) {
                        turtle_stack_touch_(stack, current);
                        if (inside != NULL) *inside = 1;
                        goto update;
                }
                current = current->prev;
        }

        /* No valid map was found. Let's try to load it */
        if ((turtle_stack_load_(stack, latitude, longitude, inside, error_) !=
            TURTLE_RETURN_SUCCESS) || ((inside != NULL) && (*inside == 0))) {
                /* The requested map is not available. Let's record this */
                client_release(client, 0, error_);
                client->index_la = (int)latitude;
                client->index_lo = (int)longitude;
                goto unlock;
        }
        current = stack->head;
        hx = (longitude - current->meta.x0) / current->meta.dx;
        hy = (latitude - current->meta.y0) / current->meta.dy;

/* Update the client */
update:
        if (client_release(client, 0, error_) != TURTLE_RETURN_SUCCESS)
                goto unlock;
        current->clients++;
        client->map = current;
        client->index_la = INT_MIN;
        client->index_lo = INT_MIN;

/* Unlock the stack */
unlock:
        if ((stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_ERROR_UNLOCK();
        if ((error_->code != TURTLE_RETURN_SUCCESS) ||
            ((inside != NULL) && (*inside == 0))) {
                *elevation = 0.;
                return TURTLE_ERROR_RAISE();
        }

        /* Interpolate the elevation */
interpolate:
        return turtle_map_elevation_(
            client->map, longitude, latitude, elevation, inside, error_);
}

/* Release any active map */
static enum turtle_return client_release(struct turtle_client * client,
    int lock, struct turtle_error_context * error_)
{
        if (client->map == NULL) return TURTLE_RETURN_SUCCESS;

        /* Lock the stack */
        struct turtle_stack * stack = client->stack;
        if (lock && (stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_LOCK_ERROR, "could not acquire the lock");

        /* Update the reference count */
        struct turtle_map * map = client->map;
        client->map = NULL;
        map->clients--;
        if (map->clients < 0) {
                map->clients = 0;
                TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_LIBRARY_ERROR, "an unexpected error occured");
                goto unlock;
        }

        /* Remove the map if it is unused and if there is a stack overflow */
        if ((map->clients == 0) && (stack->size > stack->max_size))
                turtle_map_destroy(&map);

/* Unlock and return */
unlock:
        if (lock && (stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_UNLOCK_ERROR, "could not release the lock");
        return error_->code;
}

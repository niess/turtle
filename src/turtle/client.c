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
 * turtle_datum.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "turtle.h"
#include "client.h"
#include "datum.h"
#include "return.h"

/* Management routine(s). */
static enum turtle_return client_release(
    struct turtle_client * client, int lock);

/* Create a new datum client. */
enum turtle_return turtle_client_create(
    struct turtle_datum * datum, struct turtle_client ** client)
{
        /* Check that one has a valid datum. */
        *client = NULL;
        if ((datum == NULL) || (datum->lock == NULL))
                TURTLE_RETURN(TURTLE_RETURN_BAD_ADDRESS, turtle_client_create);

        /* Allocate the new client and initialise it. */
        *client = malloc(sizeof(**client));
        if (*client == NULL)
                TURTLE_RETURN(TURTLE_RETURN_MEMORY_ERROR, turtle_client_create);
        (*client)->datum = datum;
        (*client)->tile = NULL;
        (*client)->index_la = INT_MIN;
        (*client)->index_lo = INT_MIN;

        return TURTLE_RETURN_SUCCESS;
}

/* Destroy a client. */
enum turtle_return turtle_client_destroy(struct turtle_client ** client)
{
        /* Check if the client has already been released. */
        if ((client == NULL) || (*client == NULL)) return TURTLE_RETURN_SUCCESS;

        /* Release any active tile. */
        enum turtle_return rc = client_release(*client, 1);
        if (rc != TURTLE_RETURN_SUCCESS)
                TURTLE_RETURN(rc, turtle_client_destroy);

        /* Free the memory and return. */
        free(*client);
        *client = NULL;

        return TURTLE_RETURN_SUCCESS;
}

/* Clear the client's memory. */
enum turtle_return turtle_client_clear(struct turtle_client * client)
{
        TURTLE_RETURN(client_release(client, 1), turtle_client_clear);
}

/* Supervised access to the elevation data. */
enum turtle_return turtle_client_elevation(struct turtle_client * client,
    double latitude, double longitude, double * elevation)
{
        /* Get the proper tile. */
        struct datum_tile * current = client->tile;
        double hx = 0., hy = 0.; /* Patch a non relevant warning. */
        int ix, iy;

        if (current != NULL) {
                /* First let's check the current tile. */
                hx = (longitude - current->x0) / current->dx;
                hy = (latitude - current->y0) / current->dy;

                if ((hx >= 0.) && (hx <= current->nx) && (hy >= 0.) &&
                    (hy <= current->ny))
                        goto interpolate;
        } else if (((int)latitude == client->index_la) &&
            ((int)longitude == client->index_lo))
                TURTLE_RETURN(
                    TURTLE_RETURN_PATH_ERROR, turtle_client_elevation);

        /* Lock the datum. */
        struct turtle_datum * datum = client->datum;
        if ((datum->lock != NULL) && (datum->lock() != 0))
                TURTLE_RETURN(
                    TURTLE_RETURN_LOCK_ERROR, turtle_client_elevation);
        enum turtle_return rc = TURTLE_RETURN_SUCCESS;

        /* The requested coordinates are not in the current tile. Let's check
         * the full stack.
         */
        current = datum->stack;
        while (current != NULL) {
                if (current == client->tile) {
                        current = current->prev;
                        continue;
                }
                hx = (longitude - current->x0) / current->dx;
                hy = (latitude - current->y0) / current->dy;
                if ((hx >= 0.) && (hx <= current->nx) && (hy >= 0.) &&
                    (hy <= current->ny)) {
                        datum_tile_touch(datum, current);
                        goto update;
                }
                current = current->prev;
        }

        /* No valid tile was found. Let's try to load it. */
        if ((rc = datum_tile_load(datum, latitude, longitude)) !=
            TURTLE_RETURN_SUCCESS) {
                /* The requested tile is not available. Let's record this.  */
                client_release(client, 0);
                client->index_la = (int)latitude;
                client->index_lo = (int)longitude;
                goto unlock;
        }
        current = datum->stack;
        hx = (longitude - current->x0) / current->dx;
        hy = (latitude - current->y0) / current->dy;

/* Update the client. */
update:
        if ((rc = client_release(client, 0)) != TURTLE_RETURN_SUCCESS)
                goto unlock;
        current->clients++;
        client->tile = current;
        client->index_la = INT_MIN;
        client->index_lo = INT_MIN;

/* Unlock the datum. */
unlock:
        if ((datum->unlock != NULL) && (datum->unlock() != 0))
                TURTLE_RETURN(
                    TURTLE_RETURN_UNLOCK_ERROR, turtle_client_elevation);
        if (rc != TURTLE_RETURN_SUCCESS) {
                *elevation = 0.;
                TURTLE_RETURN(rc, turtle_client_elevation);
        }

/* Interpolate the elevation. */
interpolate:
        ix = (int)hx;
        iy = (int)hy;
        hx -= ix;
        hy -= iy;
        const int nx = client->tile->nx;
        const int ny = client->tile->ny;
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        int ix1 = (ix >= nx - 1) ? nx - 1 : ix + 1;
        int iy1 = (iy >= ny - 1) ? ny - 1 : iy + 1;
        struct datum_tile * tile = client->tile;
        datum_tile_cb * zm = tile->z;
        *elevation = zm(tile, ix, iy) * (1. - hx) * (1. - hy) +
            zm(tile, ix, iy1) * (1. - hx) * hy +
            zm(tile, ix1, iy) * hx * (1. - hy) + zm(tile, ix1, iy1) * hx * hy;
        return TURTLE_RETURN_SUCCESS;
}

/* Release any active tile. */
static enum turtle_return client_release(
    struct turtle_client * client, int lock)
{
        if (client->tile == NULL) return TURTLE_RETURN_SUCCESS;

        /* Lock the datum. */
        struct turtle_datum * datum = client->datum;
        if (lock && (datum->lock != NULL) && (datum->lock() != 0))
                return TURTLE_RETURN_LOCK_ERROR;
        enum turtle_return rc = TURTLE_RETURN_SUCCESS;

        /* Update the reference count. */
        struct datum_tile * tile = client->tile;
        client->tile = NULL;
        tile->clients--;
        if (tile->clients < 0) {
                tile->clients = 0;
                rc = TURTLE_RETURN_LIBRARY_ERROR;
                goto unlock;
        }

        /* Remove the tile if it is unused and if there is a stack overflow. */
        if ((tile->clients == 0) && (datum->stack_size > datum->max_size))
                datum_tile_destroy(datum, tile);

/* Unlock and return. */
unlock:
        if (lock && (datum->unlock != NULL) && (datum->unlock() != 0))
                return TURTLE_RETURN_UNLOCK_ERROR;
        return rc;
}

/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#include <stdlib.h>
#include <string.h>

#include "turtle.h"
#include "turtle_client.h"
#include "turtle_datum.h"

/* Management routine(s). */
static enum turtle_return client_release(struct turtle_client * client,
	int lock);

/* Create a new datum client. */
enum turtle_return turtle_client_create(struct turtle_datum * datum,
	struct turtle_client ** client)
{
	/* Check that one has a valid datum. */
	*client = NULL;
	if (datum == NULL) return TURTLE_RETURN_DOMAIN_ERROR;

	/* Allocate the new client and initialise it. */
	*client = malloc(sizeof(**client));
	if (*client == NULL) return TURTLE_RETURN_MEMORY_ERROR;
	(*client)->datum = datum;
	(*client)->tile = NULL;

	return TURTLE_RETURN_SUCCESS;
}

/* Destroy a client. */
enum turtle_return turtle_client_destroy(struct turtle_client ** client)
{
	/* Check if the client has already been released. */
	if ((client == NULL) || (*client == NULL))
		return TURTLE_RETURN_SUCCESS;

	/* Release any active tile. */
	enum turtle_return rc = client_release(*client, 1);
	if (rc != TURTLE_RETURN_SUCCESS) return rc;

	/* Free the memory and return. */
	free(*client);
	*client = NULL;

	return TURTLE_RETURN_SUCCESS;
}

/* Clear the client's memory. */
enum turtle_return turtle_client_clear(struct turtle_client * client)
{
	return client_release(client, 1);
}

/* Supervised access to the elevation data. */
enum turtle_return turtle_client_elevation(struct turtle_client * client,
	double latitude, double longitude, double * elevation)
{
	/* Get the proper tile. */
	struct datum_tile * current = client->tile;
	double hx, hy;
	int ix, iy;

	if (current != NULL) {
		/* First let's check the current tile. */
		hx = (longitude-current->x0)/current->dx;
		hy = (latitude-current->y0)/current->dy;

		if ((hx >= 0.) && (hx <= current->nx) && (hy >= 0.) &&
			(hy <= current->ny))
			goto interpolate;
	}

	/* Lock the datum. */
	struct turtle_datum * datum = client->datum;
	if ((datum->lock != NULL) && (datum->lock() != 0))
		return TURTLE_RETURN_LOCK_ERROR;
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
		hx = (longitude-current->x0)/current->dx;
		hy = (latitude-current->y0)/current->dy;
		if ((hx >= 0.) && (hx <= current->nx) && (hy >= 0.) &&
			(hy <= current->ny)) {
				datum_tile_touch(datum, current);
				goto update;
			}
		current = current->prev;
	}

	/* No valid tile was found. Let's try to load it. */
	if ((rc = datum_tile_load(datum, latitude, longitude))
		!= TURTLE_RETURN_SUCCESS) goto unlock;
	current = datum->stack;
	hx = (longitude-current->x0)/current->dx;
	hy = (latitude-current->y0)/current->dy;

	/* Update the client. */
update:
	if ((rc = client_release(client, 0)) != TURTLE_RETURN_SUCCESS)
		goto unlock;
	current->clients++;
	client->tile = current;

	/* Unlock the datum. */
unlock:
	if ((datum->unlock != NULL) && (datum->unlock() != 0))
		return TURTLE_RETURN_UNLOCK_ERROR;
	if (rc != TURTLE_RETURN_SUCCESS) {
		*elevation = 0.;
		return rc;
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
	int ix1 = (ix >= nx-1) ? nx-1 : ix+1;
	int iy1 = (iy >= ny-1) ? ny-1 : iy+1;
 	const int16_t * zm = client->tile->z;
	*elevation = zm[iy*nx+ix]*(1.-hx)*(1.-hy)+zm[iy1*nx+ix]*(1.-hx)*hy+
		zm[iy*nx+ix1]*hx*(1.-hy)+zm[iy1*nx+ix1]*hx*hy;
	return TURTLE_RETURN_SUCCESS;
}

/* Release any active tile. */
static enum turtle_return client_release(struct turtle_client * client,
	int lock)
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

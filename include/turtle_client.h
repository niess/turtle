/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#ifndef TURTLE_CLIENT_H
#define TURTLE_CLIENT_H

/* Container for a datum client. */
struct turtle_client {
	/* The currently used tile. */
	struct datum_tile * tile;

	/* The master datum. */
	struct turtle_datum * datum;
};

#endif

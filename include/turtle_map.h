/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#ifndef TURTLE_MAP_H
#define TURTLE_MAP_H

#include "turtle_projection.h"

/* Container for a projection map. */
struct turtle_map {
	/* Map data. */
	int bit_depth;
	int nx, ny;
	double x0, y0, z0;
	double dx, dy, dz;
	void * z;

	/* Projection. */
	struct turtle_projection projection;

	char data[]; /* Placeholder for dynamic data. */
};

#endif

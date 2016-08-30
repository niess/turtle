/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#ifndef TURTLE_PROJECTION_H
#define TURTLE_PROJECTION_H

enum projection_type {
	PROJECTION_NONE = -1,
	PROJECTION_LAMBERT,
	PROJECTION_UTM,
	N_PROJECTIONS
};

/* Container for a geographic projection. */
struct turtle_projection {
	enum projection_type type;
	union {
		struct {
			double longitude_0;
			int hemisphere;
		} utm;
		int lambert_tag;
	} settings;
};

#endif

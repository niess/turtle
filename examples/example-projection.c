#include <stdlib.h>
#include <stdio.h>

#include "turtle.h"

static struct turtle_datum * datum = NULL;
static struct turtle_map * map = NULL;

void exit_gracefully(enum turtle_return rc)
{
	turtle_map_destroy(&map);
	turtle_datum_destroy(&datum);
	turtle_finalise();
	exit(rc);
}

void error_handler(enum turtle_return rc, turtle_caller_t * caller)
{
	fprintf(stderr, "error in %s (%s) : %s.\n", __FILE__,
		turtle_strfunc(caller), turtle_strerror(rc));
	exit_gracefully(rc);
}

int main()
{
	enum turtle_return rc;

	/* Initialise the TURTLE API */
	turtle_initialise(error_handler);

	/* Create the datum for ASTER-GDEM2 elevation data. */
	turtle_datum_create("../turtle-data/ASTGTM2", 4, NULL, NULL, &datum);

	/*
	 * Create the RGF93 local projection map, centered on the Auberge des
	 * Gros Manaux at Col de Ceyssat, Auvergne, France.
	 */
	const int nx = 201;
	const int ny = 201;
	struct turtle_box box = {696530.7, 6518284.5, 3000., 3000.};
	turtle_map_create("Lambert 93", &box, nx, ny, 500., 1500., 16, &map);
	struct turtle_projection * rgf93 = turtle_map_projection(map);

	/* Fill the local map with the datum elevation data. */
	int ix, iy;
	for (ix = 0; ix < nx; ix++) for (iy = 0; iy < ny; iy++) {
		/* Get the node geodetic coordinates. */
		double x, y, latitude, longitude;
		turtle_map_node(map, ix, iy, &x, &y, NULL);
		turtle_projection_unproject(rgf93, x, y, &latitude,
			&longitude);

		/* Fill the elevation. */
		double elevation;
		turtle_datum_elevation(datum, latitude, longitude, &elevation);
		turtle_map_fill(map, ix, iy, elevation);
	}

	/* Dump the projection map. */
	turtle_map_dump(map, "pdd-30m.png");

	exit_gracefully(TURTLE_RETURN_SUCCESS);
}

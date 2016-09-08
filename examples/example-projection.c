#include <stdlib.h>
#include <stdio.h>

#include "turtle.h"

/* Encapsulate the TURTLE library calls with an error check. */
static int line = -1;
#define TRY(cmd) if ((rc = cmd) != TURTLE_RETURN_SUCCESS) \
	{line = __LINE__; goto error;}

int main()
{
	enum turtle_return rc;
	struct turtle_datum * datum = NULL;
	struct turtle_map * map = NULL;
	struct turtle_projection * rgf93 = NULL;

	/* Initialise the TURTLE API */
	turtle_initialise();

	/* Create the datum for ASTER-GDEM2 elevation data. */
	TRY( turtle_datum_create("../turtle-data/ASTGTM2", 4, NULL, NULL,
		&datum) );

	/*
	 * Create the RGF93 local projection map, centered on the Auberge des
	 * Gros Manaux at Col de Ceyssat, Auvergne, France.
	 */
	const int nx = 201;
	const int ny = 201;
	struct turtle_box box = {696530.7, 6518284.5, 3000., 3000.};
	TRY( turtle_map_create("Lambert 93", &box, nx, ny,
		500., 1500., 16, &map) )
	rgf93 = turtle_map_projection(map);

	/* Fill the local map with the datum elevation data. */
	int ix, iy;
	for (ix = 0; ix < nx; ix++) for (iy = 0; iy < ny; iy++) {
		/* Get the node geodetic coordinates. */
		double x, y, latitude, longitude;
		TRY( turtle_map_node(map, ix, iy, &x, &y, NULL) )
		TRY( turtle_projection_unproject(rgf93, x, y, &latitude,
			&longitude) )

		/* Fill the elevation. */
		double elevation;
		TRY( turtle_datum_elevation(datum, latitude, longitude,
			&elevation) )
		TRY( turtle_map_fill(map, ix, iy, elevation) )
	}

	/* Dump the projection map. */
	TRY( turtle_map_dump(map, "pdd-30m.png") )

	goto clean_and_exit;
error:
	fprintf(stderr, "error in %s:%d. %s.\n", __FILE__, line,
		turtle_strerror(rc));

clean_and_exit:
	turtle_map_destroy(&map);
	turtle_datum_destroy(&datum);
	turtle_finalise();
	exit(rc);
}

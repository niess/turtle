#include <stdlib.h>
#include <stdio.h>

#include "turtle.h"

static int line = -1;
#define RAISE_ERROR {line=__LINE__; goto error;}

int main()
{
#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif
	enum turtle_return rc;
	struct turtle_map * map = NULL;
	struct turtle_projection * utm = NULL;
	struct turtle_datum * datum = NULL;

	turtle_initialise();

	/* Check the RGF93 map. */
	if ((rc = turtle_map_load("../turtle-data/LIDAR/pdd-2.5m-16b.png",
		NULL, &map)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	const struct turtle_projection * projection =
		turtle_map_projection(map);
	char * projection_tag;
	if ((rc = turtle_projection_info(projection, &projection_tag)) !=
		TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	if (projection_tag != NULL) {
		fprintf(stdout, "map projection: %s\n", projection_tag);
		free(projection_tag);
	}

	double x, y, z;
	x = 696530.7;
	y = 6518284.5;
	if ((rc = turtle_map_elevation(map, x, y, &z)) != TURTLE_RETURN_SUCCESS)
		RAISE_ERROR;
	fprintf(stdout, "elevation: %.3lf\n", z);

	double latitude, longitude;
	if ((rc = turtle_projection_unproject(projection, x, y,
		&latitude, &longitude)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "geodetic (RGF93): %.8lg, %.8lg\n", latitude, longitude);

	if ((rc = turtle_projection_project(projection, latitude, longitude,
		&x, &y)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "RGF93: %.3lf, %.3lf\n", x, y);

	/* Check the UTM projection. */
	if ((rc = turtle_projection_create("UTM 31N", &utm)) !=
		TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	if ((rc = turtle_projection_project(utm, latitude, longitude,
		&x, &y)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "UTM 31N: %.3lf, %.3lf\n", x, y);
	if ((rc = turtle_projection_unproject(utm, x, y,
		&latitude, &longitude)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "geodetic (UTM): %.8lg, %.8lg\n", latitude, longitude);

	/* Check the ASTER-GDEM2 elevation. */
	if ((rc = turtle_datum_create("../turtle-data/ASTGTM2",
		3, NULL, NULL, &datum)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;

	if ((rc = turtle_datum_elevation(datum, latitude, longitude, &z))
		!= TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "elevation: %.3lf\n", z);

	/* Check the ECEF transforms. */
	double ecef[3];
	if ((rc = turtle_datum_ecef(datum, latitude, longitude, z, ecef))
		!= TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "ECEF: (%.5lE, %.5lE, %.5lE)\n", ecef[0], ecef[1],
		ecef[2]);

	if ((rc = turtle_datum_geodetic(datum, ecef, &latitude, &longitude,
		&z)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "Geodetic: (%.8lg, %.8lg, %.3lf)\n", latitude,
		longitude, z);

	rc = TURTLE_RETURN_SUCCESS;
	goto clean_and_exit;
error:
	fprintf(stderr, "error in %s:%d. %s.\n", __FILE__, line,
		turtle_strerror(rc));

clean_and_exit:
	turtle_projection_destroy(&utm);
	turtle_map_destroy(&map);
	turtle_datum_destroy(&datum);
	turtle_finalise();
	exit(rc);
}

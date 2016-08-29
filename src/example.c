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

	if ((rc = turtle_initialise()) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;

	if ((rc = turtle_map_load("../wwmu/topography/LIDAR/pdd-2.5m-16b-prj.png",
		NULL, &map)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;

	char * projection;
	if ((rc = turtle_map_info(map, NULL, NULL, NULL, &projection)) !=
		TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	if (projection != NULL) {
		fprintf(stdout, "projection: %s\n", projection);
		free(projection);
	}

	double x, y, z;
	x = 696530.7;
	y = 6518284.5;
	if ((rc = turtle_map_elevation(map, x, y, &z)) != TURTLE_RETURN_SUCCESS)
		RAISE_ERROR;
	fprintf(stdout, "elevation: %.5lE\n", z);

	double latitude, longitude;
	if ((rc = turtle_map_unproject(map, x, y, &latitude, &longitude)) !=
		TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "geodetic: %.12lg, %.12lg\n", latitude, longitude);

	rc = TURTLE_RETURN_SUCCESS;
	goto clean_and_exit;

error:
	fprintf(stderr, "error in %s:%d. %s.\n", __FILE__, line,
		turtle_strerror(rc));

clean_and_exit:
	turtle_map_destroy(&map);
	turtle_finalise();
	exit(rc);
}

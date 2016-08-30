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

	if ((rc = turtle_initialise()) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;

	if ((rc = turtle_map_load("../wwmu/topography/LIDAR/pdd-2.5m-16b-prj.png",
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


	if ((rc = turtle_projection_create("UTM 31N", &utm)) !=
		TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	if ((rc = turtle_projection_project(utm, latitude, longitude,
		&x, &y)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "UTM 31N: %.3lf, %.3lf\n", x, y);
	if ((rc = turtle_projection_unproject(utm, x, y,
		&latitude, &longitude)) != TURTLE_RETURN_SUCCESS) RAISE_ERROR;
	fprintf(stdout, "geodetic (UTM): %.8lg, %.8lg\n", latitude, longitude);

	if ((rc = turtle_map_dump(map, "test.png")) != TURTLE_RETURN_SUCCESS)
		RAISE_ERROR;

	rc = TURTLE_RETURN_SUCCESS;
	goto clean_and_exit;

error:
	fprintf(stderr, "error in %s:%d. %s.\n", __FILE__, line,
		turtle_strerror(rc));

clean_and_exit:
	turtle_projection_destroy(&utm);
	turtle_map_destroy(&map);
	turtle_finalise();
	exit(rc);
}

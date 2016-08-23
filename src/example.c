#include <stdlib.h>
#include <stdio.h>

#include "turtle.h"

int main()
{
	if (turtle_initialise() != 0) exit(-1);
	
	struct turtle_box box = {15.231498, 38.802507, 5.E+03, 5.E+03};
	struct turtle_map * map  = turtle_load(
		"../wwmu/topography/ASTER-GDEM2", &box);
	if (map == NULL) {
		perror("couldn't load map");
		exit(EXIT_FAILURE);
	}
	turtle_info(map, &box);
	
	const double z0 = turtle_elevation(map, box.x0, box.y0);
	fprintf(stdout, "Bounding box | %.3lf %.3lf %.3lf %.3lf | %.3lf\n",
		box.x0, box.y0, box.half_x, box.half_y, z0);

	turtle_finalise();
	exit(EXIT_SUCCESS);
}

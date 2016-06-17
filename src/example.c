#include <stdlib.h>
#include <stdio.h>

#include "topo.h"

int main()
{
	if (topo_initialise() != 0) exit(-1);
	
	struct topo_box box = {15.231498, 38.802507, 5.E+03, 5.E+03};
	struct topo_map * map  = topo_load(
		"../wwmu/topography/ASTER-GDEM2", &box);
	if (map == NULL) {
		perror("couldn't load map");
		exit(EXIT_FAILURE);
	}
	topo_info(map, &box);
	
	const double z0 = topo_elevation(map, box.x0, box.y0);
	fprintf(stdout, "Bounding box | %.3lf %.3lf %.3lf %.3lf | %.3lf\n",
		box.x0, box.y0, box.half_x, box.half_y, z0);

	topo_finalise();
	exit(EXIT_SUCCESS);
}

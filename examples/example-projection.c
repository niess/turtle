/**
 * This example shows how to project data from a Global Digital Elevation Model
 * (GDEM) onto a local map dumped to disk.
 */

/* C89 standard library. */
#include <stdio.h>
#include <stdlib.h>

/* The TURTLE API. */
#include "turtle.h"

/**
 * The stack and the projection map handles are declared globally. This allows
 * us to define a simple error handler with a gracefull exit to the OS.
 */
static struct turtle_stack * stack = NULL;
static struct turtle_map * map = NULL;

/* Clean all allocated data and exit to the OS. */
void exit_gracefully(enum turtle_return rc)
{
        turtle_map_destroy(&map);
        turtle_stack_destroy(&stack);
        turtle_finalise();
        exit(rc);
}

/* User supplied error handler. */
void error_handler(
    enum turtle_return rc, turtle_function_t * caller, const char * message)
{
        fprintf(stderr, "error: %s.\n", message);
        exit_gracefully(rc);
}

/**
 * Let's do the job now. First a `turtle_stack` is created in order to access
 * the elevation data. Then we create an empty `turtle_map` for the projected
 * elevation data. Following we loop over the map nodes and fill the elevation
 * values from the GDEM. Finnaly the resulting map is dumped to disk.
 *
 * __Warning__
 *
 * For this example to work you'll need elevation data tiles (e.g. ASTER-GDEM2,
 * or SRTM) to be located in a folder named `share/topography`.
 */
int main()
{
        /* Initialise the TURTLE API */
        turtle_initialise(error_handler);

        /* Create the stack. */
        turtle_stack_create(&stack, "share/topography", 1, NULL, NULL);

        /*
         * Create a RGF93 local projection map, centered on the Auberge des
         * Gros Manaux at Col de Ceyssat, Auvergne, France.
         */
        const int nx = 201;
        const int ny = 201;
        struct turtle_map_info info = { .nx = 201, .ny = 201,
            .x = { 693530.7, 699530.7 }, .y = { 6515284.5, 6521284.5 },
            .z = { 500., 1500. }};
        turtle_map_create(&map, &info, "Lambert 93");
        struct turtle_projection * rgf93 = turtle_map_projection(map);

        /* Fill the local map with the GDEM data. */
        int ix, iy;
        for (ix = 0; ix < nx; ix++)
                for (iy = 0; iy < ny; iy++) {
                        /* Get the node geodetic coordinates. */
                        double x, y, latitude, longitude;
                        turtle_map_node(map, ix, iy, &x, &y, NULL);
                        turtle_projection_unproject(
                            rgf93, x, y, &latitude, &longitude);

                        /* Fill the elevation. */
                        double elevation;
                        turtle_stack_elevation(
                            stack, latitude, longitude, &elevation, NULL);
                        turtle_map_fill(map, ix, iy, elevation);
                }

        /* Dump the projection map. */
        turtle_map_dump(map, "pdd-30m.png");

        exit_gracefully(TURTLE_RETURN_SUCCESS);
}

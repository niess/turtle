/**
 * This example shows how to project the global elevation data from ASTER-GDEM2
 * onto a local map dumped to disk.
 */

/* C89 standard library. */
#include <stdio.h>
#include <stdlib.h>

/* The TURTLE API. */
#include "turtle.h"

/**
 * The geodetic datum and the projection map handles are declared globally.
 * This allows us to define a simple error handler with a gracefull exit to
 * the OS.
 */
static struct turtle_datum * datum = NULL;
static struct turtle_map * map = NULL;

/* Clean all allocated data and exit to the OS. */
void exit_gracefully(enum turtle_return rc)
{
        turtle_map_destroy(&map);
        turtle_datum_destroy(&datum);
        turtle_finalise();
        exit(rc);
}

/* User supplied error handler. */
void error_handler(
    enum turtle_return rc, turtle_caller_t * caller, const char * message)
{
        fprintf(stderr, "error: %s.\n", message);
        exit_gracefully(rc);
}

/**
 * Let's do the job now. First a `turtle_datum` is created in order to access
 * the ASTER-GDEM2 data. Then we create an empty `turtle_map` for the projected
 * elevation data. Following we loop over the map nodes and fill the elevation
 * values from the datum. Finnaly the resulting map is dumped to disk.
 *
 * __Warning__
 *
 * For this example to work you'll need the `ASTGMT2_N45E002_dem.tif` elevation
 * data tile to be located in a folder named `ASTGTM2`.
 */
int main()
{
        /* Initialise the TURTLE API */
        turtle_initialise(error_handler);

        /* Create the datum for elevation data. */
        turtle_datum_create("share/topography", 1, NULL, NULL, &datum);

        /*
         * Create a RGF93 local projection map, centered on the Auberge des
         * Gros Manaux at Col de Ceyssat, Auvergne, France.
         */
        const int nx = 201;
        const int ny = 201;
        struct turtle_box box = { 696530.7, 6518284.5, 3000., 3000. };
        turtle_map_create("Lambert 93", &box, nx, ny, 500., 1500., 16, &map);
        struct turtle_projection * rgf93 = turtle_map_projection(map);

        /* Fill the local map with the datum elevation data. */
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
                        turtle_datum_elevation(
                            datum, latitude, longitude, &elevation);
                        turtle_map_fill(map, ix, iy, elevation);
                }

        /* Dump the projection map. */
        turtle_map_dump(map, "pdd-30m.png");

        exit_gracefully(TURTLE_RETURN_SUCCESS);
}

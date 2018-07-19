/**
 * This example shows how to step through elevation data using Cartesian ECEF
 * coordinates.
 */

/* C89 standard library. */
#include <stdio.h>
#include <stdlib.h>

/* The TURTLE API. */
#include "turtle.h"

/**
 * The turtle objects are declared globally. This allows us to define a simple
 * error handler with a gracefull exit to the OS.
 */
static struct turtle_datum * datum = NULL;
static struct turtle_map * map = NULL;
static struct turtle_stepper * stepper = NULL;

/* Clean all allocated data and exit to the OS. */
void exit_gracefully(enum turtle_return rc)
{
        turtle_stepper_destroy(&stepper);
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
 * the global DEM data. Then we load a `turtle_map` of some projected
 * elevation data. Finally we instanciate a stepper and do some basic stepping.
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

        /* Create the datum for elevation data. */
        turtle_datum_create("share/topography", 1, NULL, NULL, &datum);

        /*
         * Get the RGF93 local projection map, centered on the Auberge des
         * Gros Manaux at Col de Ceyssat, Auvergne, France.
         */
        turtle_map_load("pdd-30m.png", NULL, &map);

        /* Create the ECEF stepper and configure it. */
        turtle_stepper_create(&stepper);
        turtle_stepper_range_set(stepper, 100.);
        turtle_stepper_add_flat(stepper, 0.);
        turtle_stepper_add_datum(stepper, datum);
        turtle_stepper_add_map(stepper, map);

        /* Do some stepping */
        const double latitude = 45.76415653, longitude = 2.95536402;
        double position[3], direction[3];
        turtle_datum_ecef(datum, latitude, longitude, 1080.86, position);
        turtle_datum_direction(datum, latitude, longitude, 26., 5., direction);

        double s;
        for (s = 0.; s <= 5E+03; s += 10.) {
                double r[3] = { position[0] + direction[0] * s, position[1] +
                            direction[1] * s, position[2] + direction[2] * s };
                double altitude, ground_elevation;
                int layer;
                turtle_stepper_step(stepper, r, NULL, NULL, &altitude,
                    &ground_elevation, &layer);
                printf("%2d %.5lE %.5lE %.5lE\n", layer, s, altitude,
                    ground_elevation);
        }

        exit_gracefully(TURTLE_RETURN_SUCCESS);
}

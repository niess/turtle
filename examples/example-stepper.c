/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org>
 */

/*
 * This example illustrates how to step through elevation data with a
 * `turtle_stepper`, using Cartesian ECEF coordinates.
 * 
 * Note that for this example to work you'll need the 4 tiles at (45N, 2E),
 * (45N, 3E), (46N, 2E), and (46N, 3E) from a global model, e.g. N45E002.hgt,
 * etc ... for SRTMGL1. These tiles are assumed to be located in a folder named
 * `share/topography`.
 */

/* C89 standard library */
#include <stdio.h>
#include <stdlib.h>
/* The TURTLE library */
#include "turtle.h"

/*
 * The turtle objects are declared globally. This allows us to define a simple
 * error handler with a gracefull exit to the OS.
 */
static struct turtle_map * geoid = NULL;
static struct turtle_map * map = NULL;
static struct turtle_stack * stack = NULL;
static struct turtle_stepper * stepper = NULL;

/* Clean all allocated data and exit to the OS. */
void exit_gracefully(enum turtle_return rc)
{
        turtle_stepper_destroy(&stepper);
        turtle_map_destroy(&geoid);
        turtle_map_destroy(&map);
        turtle_stack_destroy(&stack);
        turtle_finalise();
        exit(rc);
}

/* Handler for TURTLE library errors */
void error_handler(
    enum turtle_return rc, turtle_function_t * caller, const char * message)
{
        fprintf(stderr, "A TURTLE library error occurred:\n%s\n", message);
        exit_gracefully(EXIT_FAILURE);
}

int main()
{
        /* Initialise the TURTLE library */
        turtle_initialise(error_handler);

        /* Create the stack */
        turtle_stack_create(&stack, "share/topography", 1, NULL, NULL);

        /*
         * Get the RGF93 local projection map, centered on the Auberge des
         * Gros Manaux at Col de Ceyssat, Auvergne, France.
         */
        turtle_map_load(&map, "share/data/pdd-30m.png");

        /* Load the EGM96 geoid map */
        turtle_map_load(&geoid, "share/data/egm96.png");

        /* Create the ECEF stepper and configure it */
        turtle_stepper_create(&stepper);
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_range_set(stepper, 100.);
        turtle_stepper_add_flat(stepper, 0.);
        turtle_stepper_add_stack(stepper, stack);
        turtle_stepper_add_map(stepper, map);

        /* Do some stepping */
        const double latitude = 45.76415653, longitude = 2.95536402;
        double position[3], direction[3], undulation;
        turtle_map_elevation(geoid, longitude, latitude, &undulation, NULL);
        turtle_ecef_from_geodetic(
            latitude, longitude, 1080.86 + undulation, position);
        turtle_ecef_from_horizontal(latitude, longitude, 26., 5., direction);

        double s;
        for (s = 0.; s <= 5E+03; s += 10.) {
                double r[3] = { position[0] + direction[0] * s,
                        position[1] + direction[1] * s,
                        position[2] + direction[2] * s };
                double altitude, ground_elevation;
                int layer;
                turtle_stepper_step(stepper, r, NULL, NULL, &altitude,
                    &ground_elevation, &layer);
                printf("%2d %.5lE %.5lE %.5lE\n", layer, s, altitude,
                    ground_elevation);
        }

        exit_gracefully(TURTLE_RETURN_SUCCESS);
}

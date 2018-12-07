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
 * `share/topography`. In addition you'll need the local map created by
 * `example-projection` as well as a grid of the EGM96 geoid undulations
 * (`ww15mgh.grd`).
 */

/* C89 standard library */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
/* The TURTLE library */
#include "turtle.h"

/*
 * The turtle objects are declared globally. This allows us to define a simple
 * error handler with a gracefull exit to the OS. It also provides a simple
 * example of error handler with clean memory management.
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
        exit(rc);
}

/* Handler for TURTLE library errors */
void handle_error(enum turtle_return code, turtle_function_t * function,
    const char * message)
{
        fprintf(stderr, "A TURTLE library error occurred:\n%s\n", message);
        exit_gracefully(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{
        /* Parse the arguments */
        double azimuth = 26., elevation = 5.;
        double approximation_range = 10., resolution_factor = 1E-02,
            slope_factor = 1.;
        if (argc && --argc) azimuth = atof(*++argv);
        if (argc && --argc) elevation = atof(*++argv);
        if (argc && --argc) approximation_range = atof(*++argv);
        if (argc && --argc) resolution_factor = atof(*++argv);
        if (argc && --argc) slope_factor = atof(*++argv);

        /* Set a custom error handler */
        turtle_error_handler_set(&handle_error);

        /* Create the stack */
        turtle_stack_create(&stack, "share/topography", 0, NULL, NULL);

        /*
         * Get the RGF93 local projection map, centered on the Auberge des
         * Gros Manaux at Col de Ceyssat, Auvergne, France.
         */
        turtle_map_load(&map, "share/data/pdd-30m.png");

        /* Load the EGM96 geoid map */
        turtle_map_load(&geoid, "share/data/ww15mgh.grd");

        /* Create the ECEF stepper and configure it */
        turtle_stepper_create(&stepper);
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_slope_set(stepper, slope_factor);
        turtle_stepper_resolution_set(stepper, resolution_factor);
        turtle_stepper_range_set(stepper, approximation_range);
        turtle_stepper_add_flat(stepper, 0.);
        turtle_stepper_add_stack(stepper, stack, 0.);
        turtle_stepper_add_map(stepper, map, 0.);

        /* Get the initial position and direction in ECEF */
        const double latitude = 45.76415653, longitude = 2.95536402;
        const double height = 0.5, altitude_max = 2.0E+03;
        double position[3], direction[3];
        turtle_stepper_position(stepper, latitude, longitude, height,
            0, position, NULL);
        turtle_ecef_from_horizontal(latitude, longitude, azimuth, elevation,
            direction);

        double altitude, ground_elevation;
        turtle_stepper_step(stepper, position, NULL, NULL, NULL, &altitude,
            &ground_elevation, NULL, NULL);

        /* Do the stepping */
        double rock_length = 0.;
        while (altitude < altitude_max) {
                /* Check the step position */
                const int inside = altitude < ground_elevation;

                /* Do the next step */
                double step;
                turtle_stepper_step(stepper, position, direction, NULL,
                        NULL, &altitude, &ground_elevation, &step, NULL);

                /* Update the rock depth if the step started below the
                 * topography
                 */
                if (inside) rock_length += step;
        }

        /* Log the result and exit */
        printf("%.6lf\n", rock_length);
        exit_gracefully(TURTLE_RETURN_SUCCESS);
}

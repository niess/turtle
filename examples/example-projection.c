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
 * This example shows how to project data from a Global Digital Elevation Model
 * (GDEM) onto a local map dumped to disk. It also provides a simple example
 * of error handler with clean memory management.
 *
 * Note that for this example to work you'll need the tile at (45N, 2E) from
 * a global model, e.g. N45E002.hgt for SRTMGL1. This tile is assumed to be
 * located in a folder named `share/topography`.
 */

/* C89 standard library */
#include <stdio.h>
#include <stdlib.h>
/* The TURTLE library */
#include "turtle.h"

/* A stack of elevation data, from a global model, e.g. SRTMGL1,
 * ASTER-GDEM2, ...
 */
static struct turtle_stack * stack = NULL;

/* A map that will contain the projected data */
static struct turtle_map * map = NULL;

/* Clean all allocated data and exit to the OS */
void exit_gracefully(enum turtle_return rc)
{
        turtle_map_destroy(&map);
        turtle_stack_destroy(&stack);
        exit(rc);
}

/* Handler for TURTLE library errors */
void handle_error(
    enum turtle_return rc, turtle_function_t * function, const char * message)
{
        fprintf(stderr, "A TURTLE library error occurred:\n%s\n", message);
        exit_gracefully(EXIT_FAILURE);
}

int main()
{
        /* Set a custom error handler */
        turtle_error_handler_set(&handle_error);

        /* Create the stack of global elevation data */
        turtle_stack_create(&stack, "share/topography", 0, NULL, NULL);

        /*
         * Create a RGF93 local projection map, centered on the Auberge des
         * Gros Manaux at Col de Ceyssat, Auvergne, France
         */
        const int nx = 201;
        const int ny = 201;
        struct turtle_map_info info = { .nx = 201,
                .ny = 201,
                .x = { 693530.7, 699530.7 },
                .y = { 6515284.5, 6521284.5 },
                .z = { 500., 1500. } };
        turtle_map_create(&map, &info, "Lambert 93");
        const struct turtle_projection * rgf93 = turtle_map_projection(map);

        /* Fill the local map from the global data */
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

        /* Dump the projection map to disk */
        turtle_map_dump(map, "share/data/pdd-30m.png");

        /* Clean and exit */
        exit_gracefully(TURTLE_RETURN_SUCCESS);
}

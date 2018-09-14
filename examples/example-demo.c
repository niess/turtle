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
 * This example demonstrates various functionalities of the TURTLE library,
 * e.g. handling projection maps, a Global Digital Elevation Model (GDEM), and
 * frame coordinates conversions.
 *
 * Note that you'll need to run `example-projection` first in order to generate
 * the map used in this example.
 */

/* C89 standard library */
#include <stdio.h>
#include <stdlib.h>
/* The TURTLE library */
#include "turtle.h"

int main()
{
        /* Load the RGF93 map dumped by `example-projection` */
        const char * path = "share/data/pdd-30m.png";
        struct turtle_map * map;
        turtle_map_load(&map, path);
        printf("o) Loaded projection map `%s`\n", path);

        /* Show the map statistics */
        struct turtle_map_info info;
        const char * strproj;
        turtle_map_meta(map, &info, &strproj);

        const double dx = info.x[1] - info.x[0];
        const double dy = info.y[1] - info.y[0];
        const double x0 = info.x[0] + 0.5 * dx;
        const double y0 = info.y[0] + 0.5 * dy;
        printf("    + projection   :  %s\n", strproj);
        printf("    + origin       :  (%.2lf, %.2lf)\n", x0, y0);
        printf("    + size         :  %.2lf x %.2lf m^2\n", dx, dy);
        printf("    + nodes        :  %d x %d\n", info.nx, info.ny);
        printf("    + elevation    :  %.1lf -> %.1lf\n", info.z[0], info.z[1]);
        printf("    + encoding     :  %s\n", info.encoding);

        /* Get the map projection */
        const struct turtle_projection * rgf93 = turtle_map_projection(map);

        /* Convert the local coordinates of the map's origin to geodetic ones */
        double latitude, longitude;
        turtle_projection_unproject(rgf93, x0, y0, &latitude, &longitude);

        /* Convert the geodetic coordinates to UTM */
        const char * strutm = "UTM 31N";
        struct turtle_projection * utm;
        turtle_projection_create(&utm, strutm);

        double xUTM, yUTM;
        turtle_projection_project(utm, latitude, longitude, &xUTM, &yUTM);

        printf("o) The origin is located at:\n");
        printf("    + GPS          :  (%.8lf, %.8lf)\n", latitude, longitude);
        printf("    + %-12s :  (%.2lf, %.2lf)\n", strutm, xUTM, yUTM);

        /* We don't need the UTM projection anymore. So let's destroy it */
        turtle_projection_destroy(&utm);

        /* Create a new stack handle to access the global elevation data
         * used for building the map
         */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, "share/topography", 0, NULL, NULL);

        /* Get the orgine's elevation from the stack */
        double elevation_gdem;
        turtle_stack_elevation(
            stack, latitude, longitude, &elevation_gdem, NULL);

        /* Get the same from the map */
        double elevation_map;
        turtle_map_elevation(map, x0, y0, &elevation_map, NULL);

        printf("o) The origin's elevation is:\n");
        printf("    + GDEM         : %.2lf m\n", elevation_gdem);
        printf("    + RGF93 map    : %.2lf m\n", elevation_map);

        /* Finally, let us express the origin's coordinates in a Cartesian
         * frame, i.e. Earth-Centered, Earth-Fixed (ECEF). *Note* that this
         * conversion doesn't require access to the GDEM data, provided that
         * the elevation is known from elsewhere. E.g. in this case we use the
         * map's elevation value.
         */
        double ecef[3];
        turtle_ecef_from_geodetic(latitude, longitude, elevation_map, ecef);

        printf("o) The origin's ECEF coordinates are:\n");
        printf("    + GDEM         : (%.2lf, %.2lf, %.2lf)\n", ecef[0], ecef[1],
            ecef[2]);

        double azimuth = 26., elevation = 20.;
        double direction[3];
        turtle_ecef_from_horizontal(
            latitude, longitude, azimuth, elevation, direction);

        printf("o) The Puy de Dome summit is along:\n");
        printf(
            "    + Az-El        : (%.1lf, %.1lf) [deg]\n", azimuth, elevation);
        printf("    + ECEF         : (%.8lg, %.8lg, %.8lg)\n", direction[0],
            direction[1], direction[2]);

        /* Clean and exit to the OS */
        turtle_map_destroy(&map);
        turtle_stack_destroy(&stack);
        exit(EXIT_SUCCESS);
}

/**
 * This example demonstrates various functionalities of the TURTLE library,
 * e.g. handling projection maps, a geodetic datum, and frame coordinates
 * conversions. It also provides a simple example of TURTLE's error handling
 * using a user defined `turtle_handler` callback.
 */
#include <stdio.h>
#include <stdlib.h>

#include "turtle.h"

/* Error handler: dump any error message and exit to the OS. */
void exit_abruptly(
    enum turtle_return rc, turtle_caller_t * caller, const char * message)
{
        if (rc != TURTLE_RETURN_SUCCESS)
                fprintf(stderr, "error: %s.\n", message);
        exit(0);
}

int main()
{
        /** First let us initialise the TURTLE library. The `NULL` argument
         * specifies that no error handler is used. Most of TURTLE's library
         * functions return a `turtle_return` code that could be catched by a
         * user defined error handler. But, to start with let us handle
         * explicitly the return codes.
         */
        /* Initialise the TURTLE library. */
        turtle_initialise(NULL);

        /** Let us now demonstrate how to load a projection map from the disk
         * and how to access its meta-data. So you'll need to run
         * example-projection` first if you don't already have the map.
         */
        /* Load the RGF93 map dumped by `example-projection`. */
        const char * path = "pdd-30m.png";
        struct turtle_map * map;
        enum turtle_return rc = turtle_map_load(path, NULL, &map);
        if (rc != TURTLE_RETURN_SUCCESS)
                exit_abruptly(rc, (turtle_caller_t *)turtle_map_load,
                    "could not load map");

        printf("o) Loaded projection map `%s`\n", path);

        /* Get a handle to the map's projection. */
        struct turtle_projection * rgf93 = turtle_map_projection(map);

        /* Show the map statistics. */
        struct turtle_box box;
        int nx, ny, bit_depth;
        double zmin, zmax;
        char * strproj;
        turtle_map_info(map, &box, &nx, &ny, &zmin, &zmax, &bit_depth);
        turtle_projection_info(rgf93, &strproj);

        printf("    + projection   :  %s\n", strproj);
        printf("    + origin       :  (%.2lf, %.2lf)\n", box.x0, box.y0);
        printf("    + size         :  %.2lf x %.2lf m^2\n", box.half_x,
            box.half_y);
        printf("    + nodes        :  %d x %d\n", nx, ny);
        printf("    + elevation    :  %.1lf -> %.1lf (%d bits)\n", zmin, zmax,
            bit_depth);

        /*
         * Free the temporary dump of the projection's name since it isn't
         * furtrher required.
         */
        free(strproj);

        /** For the following, let us attach an error handler to TURTLE such
         * that we don't need anymore to explicitly handle return codes.
         *
         * *Note* that we could have done so right from the start by passing the
         * error handler as argument to `turtle_initialise`.
         */
        /* Set an error handler from here on. */
        turtle_handler(&exit_abruptly);

        /** Let us illustrate how to manipulate `turtle_projection`s in order to
         * do frame coordinates conversions.
         */
        /*
         * Convert the local coordinates of the map's origin to geodetic ones.
         */
        double latitude, longitude;
        turtle_projection_unproject(
            rgf93, box.x0, box.y0, &latitude, &longitude);

        /* Convert the coordinates to UTM. */
        const char * strutm = "UTM 31N";
        struct turtle_projection * utm;
        turtle_projection_create(strutm, &utm);

        double xUTM, yUTM;
        turtle_projection_project(utm, latitude, longitude, &xUTM, &yUTM);

        printf("o) The origin is located at:\n");
        printf("    + GPS          :  (%.8lf, %.8lf)\n", latitude, longitude);
        printf("    + %-12s :  (%.2lf, %.2lf)\n", strutm, xUTM, yUTM);

        /* We don't need the UTM projection anymore ... */
        turtle_projection_destroy(&utm);

        /** Let us show how to access the ASTER-GDEM2 elevation data. That for
         * we need to instanciate a geodetic `turtle_datum`. Since we are only
         * interested in a single coordinate the stack size for elevation data
         * is set to `1`, i.e. a single data file will be loaded and buffered.
         *
         * Having a new `turtle_datum` for ASTER-GDEM2 data, we can request the
         * origin's elevation and compare the result to the map's one. *Note*
         * that there might be a 1 cm difference between both due to the
         * encoding of the elevation data over 16 bits.
         */
        /*
         * Create a new geodetic datum handle to access the ASTER-GDEM2
         * elevation data.
         */
        struct turtle_datum * datum;
        turtle_datum_create("share/topography", 1, NULL, NULL, &datum);

        /* Get the orgin's elevation from the datum. */
        double elevation_ASTER;
        turtle_datum_elevation(
            datum, latitude, longitude, &elevation_ASTER, NULL);

        /* Get the same from the map. */
        double elevation_map;
        turtle_map_elevation(map, box.x0, box.y0, &elevation_map, NULL);

        printf("o) The origin's elevation is:\n");
        printf("    + ASTER-GDEM2  : %.2lf m\n", elevation_ASTER);
        printf("    + RGF93 map    : %.2lf m\n", elevation_map);

        /** Finally, let us express the origin's coordinates in a Cartesian
         * frame, i.e. Earth-Centered, Earth-Fixed (ECEF). *Note* that this
         * conversion doesn't require access to the ASTER-GDEM2 elevation data,
         * provided that the elevation is known from elsewhere. E.g. in this
         * case we use the map's elevation value.
         */
        double ecef[3];
        turtle_datum_ecef(datum, latitude, longitude, elevation_map, ecef);

        printf("o) The origin's ECEF coordinates are:\n");
        printf("    + ASTER-GDEM2  : (%.2lf, %.2lf, %.2lf)\n", ecef[0], ecef[1],
            ecef[2]);

        double azimuth = 26., elevation = 20.;
        double direction[3];
        turtle_datum_direction(
            datum, latitude, longitude, azimuth, elevation, direction);

        printf("o) The Puy de Dome summit is along:\n");
        printf(
            "    + Az-El        : (%.1lf, %.1lf) [deg]\n", azimuth, elevation);
        printf("    + ECEF         : (%.8lg, %.8lg, %.8lg)\n", direction[0],
            direction[1], direction[2]);

        /* Finalise and exit to the OS. */
        turtle_map_destroy(&map);
        turtle_datum_destroy(&datum);
        turtle_finalise();
        exit(0);
}

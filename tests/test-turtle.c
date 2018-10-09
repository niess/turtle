/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicules over Long rangEs (TURTLE)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/*
 * Unit tests for the Turtle C library
 */

/* C89 standard library */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Endianess utilities */
#include <arpa/inet.h>
#ifndef TURTLE_NO_TIFF
/* TIFF library */
#include <tiffio.h>
#endif
/* The TURTLE library */
#include "turtle.h"

static void catch_error(enum turtle_return code, turtle_function_t * function,
    const char * message)
{
        printf("%s\n", message);
}

static void test_map(void)
{
        /* Create a new map with a UTM projection */
        const double x0 = 496000, y0 = 5067000, z0 = 0., z1 = 1000;
        const int nx = 2001, ny = 2001;

        struct turtle_map * map;
        struct turtle_map_info info = { nx, ny, { x0 - 1000, x0 + 1000 },
                { y0 - 1000, y0 + 1000 }, { z0, z1 } };
        turtle_map_create(&map, &info, "UTM 31N");

        /* Fill the map with some dummy data */
        int i, k;
        for (i = 0, k = 0; i < nx; i++) {
                int j;
                for (j = 0; j < ny; j++, k++) {
                        const double z = ((k % 2) == 0) ? z0 : z1;
                        turtle_map_fill(map, i, j, z);
                }
        }

        /* Dump the map to disk and destroy it in memory */
        const char * path = "tests/map.png";
        turtle_map_dump(map, path);
        turtle_map_destroy(&map);

        /* Reload the map */
        turtle_map_load(&map, path);

        /* Check the map meta data */
        memset(&info, 0x0, sizeof(info));
        const char * projection;
        turtle_map_meta(map, &info, &projection);
        assert(info.nx == nx);
        assert(info.ny == ny);
        assert(info.x[0] == x0 - 1000);
        assert(info.x[1] == x0 + 1000);
        assert(info.y[0] == y0 - 1000);
        assert(info.y[1] == y0 + 1000);
        assert(info.z[0] == z0);
        assert(info.z[1] == z1);
        assert(strcmp(projection, "UTM 31N") == 0);

        /* Access the map data */
        double z;
        for (i = 0, k = 0; i < nx; i++) {
                int j;
                for (j = 0; j < ny; j++, k++) {
                        turtle_map_node(map, i, j, NULL, NULL, &z);
                        const double zz = ((k % 2) == 0) ? z0 : z1;
                        assert(z == zz);
                }
        }

        /* Check the interpolation */
        turtle_map_elevation(map, x0, y0, &z, NULL);
        turtle_map_elevation(map, x0 + 0.5, y0 + 0.5, &z, NULL);
        int inside;
        turtle_map_elevation(map, x0 - 1000.5, y0, &z, &inside);
        assert(inside == 0);

        /* Re-fill the map with some dummy data */
        int ix;
        for (ix = 0; ix < nx; ix++) {
                int iy;
                for (iy = 0; iy < ny; iy++) {
                        /* Get the node coordinates */
                        double x, y;
                        turtle_map_node(map, ix, iy, &x, &y, NULL);

                        /* Fill the elevation */
                        x -= x0;
                        y -= y0;
                        const double r = sqrt(x * x + y * y);
                        const double z = (r >= 1E+03) ? 0. : 1E+03 - r;
                        turtle_map_fill(map, ix, iy, z);
                }
        }

        /* Dump the map and destroy it in memory */
        turtle_map_dump(map, path);
        turtle_map_destroy(&map);

        /* Catch errors and try loading some wrong maps */
        turtle_error_handler_t * handler = turtle_error_handler_get();
        turtle_error_handler_set(&catch_error);
        turtle_map_load(&map, "nothing");
        turtle_map_load(&map, "nothing.nothing");
        turtle_map_create(&map, &info, "nothing");
        info.nx = 0;
        turtle_map_create(&map, &info, projection);
        turtle_error_handler_set(handler);
}

static void test_projection(void)
{
        /* Check the no projection case */
        struct turtle_map * map;
        struct turtle_map_info info = { 11, 11, { 45, 46 }, { 3, 4 },
                { -1, 1 } };
        turtle_map_create(&map, &info, NULL);
        assert(turtle_map_projection(map) == NULL);

        /* Load the UTM projection from the map */
        turtle_map_destroy(&map);
        const char * path = "tests/map.png";
        turtle_map_load(&map, path);
        const struct turtle_projection * utm = turtle_map_projection(map);
        assert(strcmp(turtle_projection_name(utm), "UTM 31N") == 0);

        /* Create a new empty projection */
        struct turtle_projection * projection;
        turtle_projection_create(&projection, NULL);
        assert(turtle_projection_name(projection) == NULL);

        /* Loop over known projections */
        const char * tag[] = { "Lambert I", "Lambert II", "Lambert IIe",
                "Lambert III", "Lambert IV", "Lambert 93", "UTM 31N",
                "UTM 3.0N", "UTM 31S", "UTM 3.0S" };

        int i;
        for (i = 0; i < sizeof(tag) / sizeof(*tag); i++) {
                /* Reconfigure the projection object */
                turtle_projection_configure(projection, tag[i]);
                assert(strcmp(turtle_projection_name(projection), tag[i]) == 0);

                /* Project for and back */
                const double latitude = 45.5, longitude = 3.5;
                double x, y, la, lo;
                turtle_projection_project(
                    projection, latitude, longitude, &x, &y);
                turtle_projection_unproject(projection, x, y, &la, &lo);
                assert(fabs(la - latitude) < 1E-08);
                assert(fabs(lo - longitude) < 1E-08);
        }

        /* Clean the memory */
        turtle_map_destroy(&map);
        turtle_projection_destroy(&projection);

        /* Catch errors and try creating some wrong projection */
        turtle_error_handler_t * handler = turtle_error_handler_get();
        turtle_error_handler_set(&catch_error);
        turtle_projection_create(&projection, "nothing");
        turtle_error_handler_set(handler);
}

static void test_ecef(void)
{
        /* Convert a position for and back */
        const double latitude = 45.5, longitude = 3.5, altitude = 1000.;
        double position[3];
        turtle_ecef_from_geodetic(latitude, longitude, altitude, position);

        double lla[3];
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        assert(fabs(lla[0] - latitude) < 1E-08);
        assert(fabs(lla[1] - longitude) < 1E-08);
        assert(fabs(lla[2] - altitude) < 1E-08);

        /* Convert a direction for and back */
        const double azimuth = 60., elevation = 30.;
        double direction[3];
        turtle_ecef_from_horizontal(
            latitude, longitude, azimuth, elevation, direction);
        double angle[2];
        turtle_ecef_to_horizontal(
            latitude, longitude, direction, angle, angle + 1);
        assert(fabs(angle[0] - azimuth) < 1E-08);
        assert(fabs(angle[1] - elevation) < 1E-08);

        /* Check the boundary cases */
        turtle_ecef_from_geodetic(90, 0, altitude, position);
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        assert(lla[0] == 90);
        assert(lla[1] == 0);
        assert(lla[2] == altitude);

        turtle_ecef_from_geodetic(-90, 0, altitude, position);
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        assert(lla[0] == -90);
        assert(lla[1] == 0);
        assert(lla[2] == altitude);

        turtle_ecef_from_geodetic(0, 90, altitude, position);
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        assert(lla[0] == 0);
        assert(lla[1] == 90);
        assert(lla[2] == altitude);
}

static void test_stack(void)
{
        /* Create some data tiles for the global stack */
        int k;
        for (k = 0; k < 4; k++) {
                const int nx = 1201, ny = 1201;
                double latitude0, latitude1;
                if ((k % 2) == 0) {
                        latitude0 = 45;
                        latitude1 = 46;
                } else {
                        latitude0 = 46;
                        latitude1 = 47;
                }
                double longitude0, longitude1;
                if ((k / 2) == 0) {
                        longitude0 = 2;
                        longitude1 = 3;
                } else {
                        longitude0 = 3;
                        longitude1 = 4;
                }
                struct turtle_map * map;
                struct turtle_map_info info = { nx, ny,
                        { longitude0, longitude1 }, { latitude0, latitude1 },
                        { 0, 1 } };
                turtle_map_create(&map, &info, NULL);
                int i;
                for (i = 0; i < nx; i++) {
                        int j;
                        for (j = 0; j < ny; j++) turtle_map_fill(map, i, j, 0);
                }
                if (k == 0)
                        turtle_map_dump(map, "tests/topography/45N_002E.png");
                else if (k == 1)
                        turtle_map_dump(map, "tests/topography/46N_002E.png");
                else if (k == 2)
                        turtle_map_dump(map, "tests/topography/45N_003E.png");
                else
                        turtle_map_dump(map, "tests/topography/46N_003E.png");
                turtle_map_destroy(&map);
        }

        /* Create the stack */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, "tests/topography", 3, NULL, NULL);

        /* Check some elevation value */
        double z;
        turtle_stack_elevation(stack, 45.5, 3.5, &z, NULL);
        assert(z == 0);
        turtle_stack_elevation(stack, 45.0, 3.5, &z, NULL);
        assert(z == 0);
        turtle_stack_elevation(stack, 46.5, 3.5, &z, NULL);
        assert(z == 0);
        turtle_stack_elevation(stack, 45.0, 3.5, &z, NULL);
        assert(z == 0);

        int inside;
        turtle_stack_elevation(stack, 45.5, 4.5, &z, &inside);
        assert(inside == 0);

        turtle_stack_elevation(stack, 45.5, 2.5, &z, NULL);
        assert(z == 0);
        turtle_stack_elevation(stack, 46.5, 2.5, &z, NULL);
        assert(z == 0);

        /* Check the clear and load functions */
        turtle_stack_clear(stack);
        turtle_stack_elevation(stack, 45.5, 2.5, &z, NULL);
        turtle_stack_load(stack);
        turtle_stack_clear(stack);
        turtle_stack_load(stack);
        turtle_stack_load(stack);

        /* Clean the memory */
        turtle_stack_destroy(&stack);
}

static int nothing(void) { return 0; }

static void test_client(void)
{
        /* Create a stack and its client */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, "tests/topography", 1, &nothing, &nothing);

        struct turtle_client * client;
        turtle_client_create(&client, stack);

        /* Check some elevation value */
        double z;
        turtle_client_elevation(client, 45.5, 3.5, &z, NULL);
        assert(z == 0);
        turtle_client_elevation(client, 45.0, 3.5, &z, NULL);
        assert(z == 0);
        turtle_client_elevation(client, 46.5, 3.5, &z, NULL);
        assert(z == 0);
        turtle_client_elevation(client, 45.0, 3.5, &z, NULL);
        assert(z == 0);

        int inside;
        turtle_client_elevation(client, 45.5, 4.5, &z, &inside);
        assert(inside == 0);
        turtle_client_elevation(client, 45.5, 4.5, &z, &inside);
        assert(inside == 0);

        turtle_client_elevation(client, 45.5, 2.5, &z, NULL);
        assert(z == 0);
        turtle_client_elevation(client, 46.5, 2.5, &z, NULL);
        assert(z == 0);

        /* Check the clear function */
        turtle_client_clear(client);
        turtle_client_elevation(client, 45.5, 3.5, &z, NULL);
        assert(z == 0);

        /* Clean the memory */
        turtle_client_destroy(&client);
        turtle_stack_destroy(&stack);

        /* Catch errors and try some false cases */
        turtle_error_handler_t * handler = turtle_error_handler_get();
        turtle_error_handler_set(&catch_error);
        turtle_client_create(&client, stack);
        turtle_stack_create(&stack, "tests/topography", 0, NULL, NULL);
        turtle_client_create(&client, stack);
        turtle_stack_destroy(&stack);
        turtle_stack_create(&stack, "tests/topography", 1, &nothing, &nothing);
        turtle_client_create(&client, stack);
        turtle_client_elevation(client, 45.5, 3.5, &z, NULL);
        turtle_error_handler_set(handler);

        /* Clean the memory */
        turtle_client_destroy(&client);
        turtle_stack_destroy(&stack);
}

static void test_stepper(void)
{
        /* Create the geoid data */
        const int nx = 361, ny = 181;
        struct turtle_map * geoid;
        struct turtle_map_info info = { nx, ny, { -180, 180 }, { -90, 90 },
                { -1, 1 } };
        turtle_map_create(&geoid, &info, NULL);
        int i;
        for (i = 0; i < nx; i++) {
                int j;
                for (j = 0; j < ny; j++) turtle_map_fill(geoid, i, j, -1);
        }
        turtle_map_dump(geoid, "tests/geoid.png");
        turtle_map_destroy(&geoid);

        /* Create the stepper */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, "tests/topography", 0, NULL, NULL);
        struct turtle_map * map;
        turtle_map_load(&map, "tests/map.png");
        turtle_map_load(&geoid, "tests/geoid.png");
        struct turtle_stepper * stepper;
        turtle_stepper_create(&stepper);

        /* Configure the stepper */
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_add_flat(stepper, 10);
        turtle_stepper_add_stack(stepper, stack);
        turtle_stepper_add_map(stepper, map);

        for (i = 0; i < 2; i++) {
                /* Set the approximation range */
                if (i) turtle_stepper_range_set(stepper, 100);

                /* Get the initial position and direction in ECEF */
                const double latitude = 45.5, longitude = 2.5;
                const double azimuth = 0, elevation = 0;
                const double height = -0.5, altitude_max = 1.5E+03;
                double position[3], direction[3];
                int layer;
                turtle_stepper_position(
                    stepper, latitude, longitude, height, position, &layer);
                turtle_ecef_from_horizontal(
                    latitude, longitude, azimuth, elevation, direction);

                /* Do some dummy stepping */
                for (;;) {
                        /* Update the step data */
                        double altitude, ground_elevation, la, lo;
                        turtle_stepper_step(stepper, position, NULL, &la, &lo,
                            &altitude, &ground_elevation, NULL, &layer);
                        if (altitude >= altitude_max) break;

                        double altitude1, ground_elevation1, la1, lo1;
                        double step_length;
                        int layer1;
                        turtle_stepper_step(stepper, position, NULL, &la1, &lo1,
                            &altitude1, &ground_elevation1, &step_length,
                            &layer1);
                        assert(altitude1 == altitude);
                        assert(ground_elevation1 == ground_elevation);
                        assert(la1 == la);
                        assert(lo1 == lo);
                        assert(layer1 == layer);

                        /* Update the position */
                        turtle_stepper_step(stepper, position, direction, &la,
                            &lo, &altitude, &ground_elevation, NULL, &layer);
                        if (altitude >= altitude_max) break;
                }
        }

        /* Check other geometries */
        turtle_stepper_destroy(&stepper);
        turtle_stepper_create(&stepper);
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_add_flat(stepper, 0);

        assert(turtle_stepper_geoid_get(stepper) == geoid);
        assert(turtle_stepper_range_get(stepper) == 1.);
        turtle_stepper_range_set(stepper, 10.);
        assert(turtle_stepper_range_get(stepper) == 10.);
        assert(turtle_stepper_slope_get(stepper) == 0.4);
        turtle_stepper_slope_set(stepper, 1.);
        assert(turtle_stepper_slope_get(stepper) == 1.);
        assert(turtle_stepper_resolution_get(stepper) == 1E-02);
        turtle_stepper_resolution_set(stepper, 1E-03);
        assert(turtle_stepper_resolution_get(stepper) == 1E-03);

        const double latitude = 45.5, longitude = 2.5, height = 0.5;
        double position[3];
        int layer;
        turtle_stepper_position(
            stepper, latitude, longitude, height, position, &layer);

        double altitude, ground_elevation, la, lo;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            &ground_elevation, NULL, &layer);
        assert(fabs(ground_elevation + height - altitude) < 1E-08);

        turtle_stepper_destroy(&stepper);
        turtle_stepper_create(&stepper);
        turtle_stepper_add_stack(stepper, stack);
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            &ground_elevation, NULL, &layer);
        assert(fabs(ground_elevation + height - altitude) < 1E-08);

        double direction[3];
        turtle_ecef_from_horizontal(latitude, longitude, 0, 0, direction);
        for (i = 0; i < 3; i++) position[i] += direction[i] * 1E+06;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            &ground_elevation, NULL, &layer);
        assert(layer == -1);

        turtle_stepper_position(stepper, 80, 0, height, position, &layer);
        assert(layer == -1);

        /* Check the client layer */
        turtle_stepper_destroy(&stepper);
        turtle_stack_destroy(&stack);
        turtle_stack_create(&stack, "tests/topography", 1, &nothing, &nothing);
        turtle_stepper_create(&stepper);
        turtle_stepper_add_stack(stepper, stack);
        turtle_stepper_range_set(stepper, 100);

        turtle_stepper_position(
            stepper, latitude, longitude, height, position, &layer);
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            &ground_elevation, NULL, &layer);
        assert(fabs(ground_elevation + height - altitude) < 1E-08);

        double altitude1, ground_elevation1, la1, lo1;
        int layer1;
        turtle_stepper_step(stepper, position, NULL, &la1, &lo1, &altitude1,
            &ground_elevation1, NULL, &layer1);
        assert(altitude1 == altitude);
        assert(ground_elevation1 == ground_elevation);
        assert(la1 == la);
        assert(lo1 == lo);
        assert(layer1 == layer);

        for (i = 0; i < 3; i++) position[i] += direction[i] * 10;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            &ground_elevation, NULL, &layer);
        assert(ground_elevation == 0);

        for (i = 0; i < 3; i++) position[i] += direction[i] * 100;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            &ground_elevation, NULL, &layer);
        assert(ground_elevation == 0);

        /* Clean the memory */
        turtle_stepper_destroy(&stepper);
        turtle_map_destroy(&geoid);
        turtle_map_destroy(&map);
        turtle_stack_destroy(&stack);
}

#ifndef TURTLE_NO_GRD
static void test_io_grd(void)
{
        /* Generate a geoid in .GRD format, e.g. as used by EGM96 */
        FILE * fid = fopen("tests/geoid.grd", "w+");
        fprintf(fid,
            "   -90.000000   90.000000     .000000  360.000000   "
            "15.000000   30.000000\n\n");

#ifndef M_PI
        /* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif
        const double deg = M_PI / 180.;

        int i, k;
        for (i = 0, k = 0; i < 13; i++) {
                const double latitude = i * 15 - 90;
                const double c = cos(latitude * deg);
                int j;
                for (j = 0; j < 13; j++, k++) {
                        const double longitude = j * 30;
                        const double undulation =
                            100 * c * cos(longitude * deg);
                        if ((k % 8) == 0) fputs(" ", fid);
                        fprintf(fid, " %8.3f", undulation);
                        if ((k % 8) == 7) fputs("\n", fid);
                        if ((k % 160) == 159) fputs("\n", fid);
                }
        }
        fclose(fid);

        /* Read back the geoid using TURTLE */
        struct turtle_map * geoid;
        turtle_map_load(&geoid, "tests/geoid.grd");

        for (i = 0; i < 13; i++) {
                const double latitude = i * 15 - 90;
                const double c = cos(latitude * deg);
                int j;
                for (j = 0; j < 13; j++) {
                        const double longitude = j * 30;
                        const double undulation =
                            100 * c * cos(longitude * deg);
                        double undulation1;
                        turtle_map_elevation(
                            geoid, longitude, latitude, &undulation1, NULL);
                        assert(fabs(undulation1 - undulation) < 1E-02);
                }
        }

        /* Check the writing to a GRD map */
        turtle_map_fill(geoid, 0, 0, 1);
        double undulation;
        turtle_map_elevation(geoid, 0, -90, &undulation, NULL);
        assert(fabs(undulation - 1) < 1E-02);

        /* Clean the memory */
        turtle_map_destroy(&geoid);
}
#endif

#ifndef TURTLE_NO_HGT
static void test_io_hgt(void)
{
        /* Generate an HGT map, e.g. used by SRTMGL1 */
        FILE * fid = fopen("tests/N45E003.hgt", "wb+");
        int i, k;
        for (i = 0, k = 0; i < 3601; i++) {
                int16_t z[3601];
                int j;
                for (j = 0; j < 3601; j++, k++) {
                        z[j] = (int16_t)htons(((k % 2) == 0) ? -1 : 1);
                }
                fwrite(z, sizeof(*z), 3601, fid);
        }
        fclose(fid);

        /* Read back the map using TURTLE */
        struct turtle_map * map;
        turtle_map_load(&map, "tests/N45E003.hgt");

        for (i = 0, k = 0; i < 3601; i++) {
                int j;
                for (j = 0; j < 3601; j++, k++) {
                        double x, y, z;
                        turtle_map_node(map, j, i, &x, &y, &z);
                        const double z1 = ((k % 2) == 0) ? -1 : 1;
                        assert(fabs(z - z1) < 1E-02);
                }
        }

        /* Check the writing to an HGT map */
        turtle_map_fill(map, 0, 0, 10);
        double z;
        turtle_map_elevation(map, 3, 45, &z, NULL);
        assert(fabs(z - 10) < 1E-02);

        /* Clean the memory */
        turtle_map_destroy(&map);
}
#endif

#ifndef TURTLE_NO_TIFF
static void test_io_tiff(void)
{
        /* Generate a GeoTIFF map */
        const char * path = "tests/map.tif";
        const int nx = 101, ny = 101;
        const double x0 = 3, x1 = 4, y0 = 45, y1 = 46;
        struct turtle_map * map;
        struct turtle_map_info info = { nx, ny, { x0, x1 }, { y0, y1 },
                { -32767, 32768 } };
        turtle_map_create(&map, &info, NULL);

        int i, k;
        for (i = 0, k = 0; i < ny; i++) {
                int j;
                for (j = 0; j < nx; j++, k++) {
                        double x, y;
                        turtle_map_node(map, j, i, &x, &y, NULL);
                        const double z = ((k % 2) == 0) ? -1 : 1;
                        turtle_map_fill(map, j, i, z);
                }
        }

        turtle_map_dump(map, path);
        turtle_map_destroy(&map);

        /* Read back the map using TURTLE */
        turtle_map_load(&map, path);

        for (i = 0, k = 0; i < ny; i++) {
                int j;
                for (j = 0; j < nx; j++, k++) {
                        double x, y, z;
                        turtle_map_node(map, j, i, &x, &y, &z);
                        const double z1 = ((k % 2) == 0) ? -1 : 1;
                        assert(z == z1);
                }
        }

        /* Check the writing to a GEOTIFF map */
        turtle_map_fill(map, 0, 0, 10);
        double z;
        turtle_map_elevation(map, 3, 45, &z, NULL);
        assert(z == 10);

        /* Clean the memory */
        turtle_map_destroy(&map);
}
#endif

static void test_strfunc(void)
{
#define CHECK_API(FUNCTION)                                                    \
        assert(turtle_error_function((turtle_function_t *)&FUNCTION) != NULL)

        CHECK_API(turtle_client_clear);
        CHECK_API(turtle_client_create);
        CHECK_API(turtle_client_destroy);
        CHECK_API(turtle_client_elevation);

        CHECK_API(turtle_ecef_from_geodetic);
        CHECK_API(turtle_ecef_from_horizontal);
        CHECK_API(turtle_ecef_to_geodetic);
        CHECK_API(turtle_ecef_to_horizontal);

        CHECK_API(turtle_error_function);
        CHECK_API(turtle_error_handler_get);
        CHECK_API(turtle_error_handler_set);

        CHECK_API(turtle_map_create);
        CHECK_API(turtle_map_destroy);
        CHECK_API(turtle_map_dump);
        CHECK_API(turtle_map_elevation);
        CHECK_API(turtle_map_fill);
        CHECK_API(turtle_map_load);
        CHECK_API(turtle_map_meta);
        CHECK_API(turtle_map_node);
        CHECK_API(turtle_map_projection);

        CHECK_API(turtle_projection_configure);
        CHECK_API(turtle_projection_create);
        CHECK_API(turtle_projection_destroy);
        CHECK_API(turtle_projection_name);
        CHECK_API(turtle_projection_project);
        CHECK_API(turtle_projection_unproject);

        CHECK_API(turtle_stack_clear);
        CHECK_API(turtle_stack_create);
        CHECK_API(turtle_stack_destroy);
        CHECK_API(turtle_stack_elevation);
        CHECK_API(turtle_stack_load);

        CHECK_API(turtle_stepper_add_flat);
        CHECK_API(turtle_stepper_add_map);
        CHECK_API(turtle_stepper_add_stack);
        CHECK_API(turtle_stepper_create);
        CHECK_API(turtle_stepper_destroy);
        CHECK_API(turtle_stepper_geoid_get);
        CHECK_API(turtle_stepper_geoid_set);
        CHECK_API(turtle_stepper_range_get);
        CHECK_API(turtle_stepper_range_set);
        CHECK_API(turtle_stepper_position);
        CHECK_API(turtle_stepper_step);

        assert(
            turtle_error_function((turtle_function_t *)&test_strfunc) == NULL);
#undef CHECK_API
}

int main()
{
        /* Run the tests */
        test_map();
        test_projection();
        test_ecef();
        test_stack();
        test_client();
        test_stepper();
#ifndef TURTLE_NO_GRD
        test_io_grd();
#endif
#ifndef TURTLE_NO_HGT
        test_io_hgt();
#endif
#ifndef TURTLE_NO_TIFF
        test_io_tiff();
#endif
        test_strfunc();

        /* Exit to the OS */
        exit(EXIT_SUCCESS);
}

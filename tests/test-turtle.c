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
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* POSIX regular expressions */
#include <regex.h> 
/* The Check library */
#include "check.h"
/* Endianess utilities */
#include <arpa/inet.h>
#ifndef TURTLE_NO_TIFF
/* TIFF library */
#include <tiffio.h>
#endif
/* The TURTLE library */
#include "turtle.h"
/* Opaque TURTLE data */
#include "../src/turtle/list.h"
#include "../src/turtle/stack.h"
#include "../src/turtle/stepper.h"


/* Shortcut(s) for checking internal type(s) */
#define CHECK_LIST_INIT(L)                                                     \
        ck_assert_int_eq(L.size, 0);                                           \
        ck_assert_ptr_eq(L.head, NULL);                                        \
        ck_assert_ptr_eq(L.tail, NULL)


/* Buffer for redirecting error messages */
static char error_buffer[2048];

static void catch_error(enum turtle_return code, turtle_function_t * function,
    const char * message)
{
        sprintf(error_buffer, "%s\n", message);
}


static void setup_map_data(void)
{
        /* Create a new map with a UTM projection */
#define DEFINE_MAP_META()                                                      \
        const double x0 = 496000, y0 = 5067000, z0 = 0., z1 = 1000;            \
        const int nx = 201, ny = 201

        DEFINE_MAP_META();
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
#define MAP_PATH "tests/map.png"
        const char * path = MAP_PATH;
        turtle_map_dump(map, path);
        turtle_map_destroy(&map);
}


static void setup_stack_data(void)
{
#define STACK_PATH "tests/topography"

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
                        turtle_map_dump(map, STACK_PATH "/45N_002E.png");
                else if (k == 1)
                        turtle_map_dump(map, STACK_PATH "/46N_002E.png");
                else if (k == 2)
                        turtle_map_dump(map, STACK_PATH "/45N_003E.png");
                else
                        turtle_map_dump(map, STACK_PATH "/46N_003E.png");
                turtle_map_destroy(&map);
        }
}


static void setup_geoid_data(void)
{
#define GEOID_PATH "tests/geoid.png"

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
        turtle_map_dump(geoid, GEOID_PATH);
        turtle_map_destroy(&geoid);
}


static void setup_data(void)
{
        setup_map_data();
        setup_stack_data();
        setup_geoid_data();
}


START_TEST (test_list)
{
        struct turtle_list list = { 0 };
        struct turtle_list_element * e0 = malloc(sizeof(*e0));
        struct turtle_list_element * e1 = malloc(sizeof(*e1));
        struct turtle_list_element * e2 = malloc(sizeof(*e2));
        struct turtle_list_element * e3 = malloc(sizeof(*e3));
        struct turtle_list_element * e4 = malloc(sizeof(*e4));

        turtle_list_append_(&list, e0);
        ck_assert_ptr_eq(list.head, e0);
        ck_assert_ptr_eq(list.tail, e0);
        ck_assert_ptr_eq(e0->previous, NULL);
        ck_assert_ptr_eq(e0->next, NULL);

        turtle_list_insert_(&list, e1, 0);
        ck_assert_ptr_eq(list.head, e1);
        ck_assert_ptr_eq(e1->next, e0);
        ck_assert_ptr_eq(e1->previous, NULL);
        ck_assert_ptr_eq(e0->previous, e1);

        turtle_list_append_(&list, e2);
        ck_assert_ptr_eq(list.tail, e2);
        ck_assert_ptr_eq(e2->previous, e0);
        ck_assert_ptr_eq(e0->next, e2);
        ck_assert_ptr_eq(e2->next, NULL);

        turtle_list_insert_(&list, e3, 1);
        ck_assert_ptr_eq(list.head, e1);
        ck_assert_ptr_eq(e0->previous, e3);
        ck_assert_ptr_eq(e3->previous, e1);
        ck_assert_ptr_eq(e3->next, e0);
        ck_assert_ptr_eq(e1->next, e3);

        turtle_list_insert_(&list, e4, 5);
        ck_assert_ptr_eq(list.tail, e4);
        ck_assert_ptr_eq(e4->previous, e2);
        ck_assert_ptr_eq(e4->next, NULL);
        ck_assert_ptr_eq(e2->next, e4);
        ck_assert_int_eq(list.size, 5);

        turtle_list_remove_(&list, e1);
        free(e1);
        ck_assert_ptr_eq(e3->previous, NULL);
        ck_assert_ptr_eq(e3->next, e0);

        turtle_list_remove_(&list, e4);
        free(e4);
        ck_assert_ptr_eq(e3->previous, NULL);
        ck_assert_ptr_eq(e3->next, e0);

        turtle_list_remove_(&list, e0);
        free(e0);
        ck_assert_int_eq(list.size, 2);
        ck_assert_ptr_eq(e3->previous, NULL);
        ck_assert_ptr_eq(e3->next, e2);
        ck_assert_ptr_eq(e2->previous, e3);
        ck_assert_ptr_eq(e2->next, NULL);

        struct turtle_list_element * e = turtle_list_pop_(&list);
        ck_assert_ptr_eq(e, e2);
        ck_assert_ptr_eq(e3->next, NULL);
        ck_assert_int_eq(list.size, 1);
        ck_assert_ptr_eq(list.head, e3);
        ck_assert_ptr_eq(list.tail, e3);

        e = turtle_list_pop_(&list);
        ck_assert_ptr_eq(e, e3);
        ck_assert_ptr_eq(list.head, NULL);
        ck_assert_ptr_eq(list.tail, NULL);
        ck_assert_int_eq(list.size, 0);

        turtle_list_append_(&list, e2);
        turtle_list_append_(&list, e3);
        turtle_list_clear_(&list);
        ck_assert_ptr_eq(list.head, NULL);
        ck_assert_ptr_eq(list.tail, NULL);
        ck_assert_int_eq(list.size, 0);
}
END_TEST


START_TEST (test_stepper_layer)
{
        struct turtle_map * map, * geoid;
        struct turtle_stack * stack;
        struct turtle_stepper * stepper;

        turtle_map_load(&map, MAP_PATH);
        turtle_map_load(&geoid, GEOID_PATH);
        turtle_stack_create(&stack, STACK_PATH, 0, NULL, NULL);

        /* Check the initialisation */
        turtle_stepper_create(&stepper);
        CHECK_LIST_INIT(stepper->data);
        CHECK_LIST_INIT(stepper->transforms);
        CHECK_LIST_INIT(stepper->layers);

        /* Check the building of geometry layers */
        const double latitude0 = 45.756546; 
        const double longitude0 = 2.9485671;
        const double offset[2] = { -0.5, 0. };
        double values[3][2];
        int i;
        for (i = 0; i < 2; i++) {
                turtle_stepper_add_layer(stepper);
                turtle_stepper_add_flat(stepper, offset[i]);
                turtle_stepper_add_stack(stepper, stack, offset[i]);
                turtle_stepper_add_map(stepper, map, offset[i]);

                struct turtle_stepper_layer * layer = stepper->layers.tail;
                ck_assert_int_eq(layer->meta.size, 3);

                double position[3], altitude;
                int index[2];
                turtle_stepper_position(stepper, latitude0, longitude0, -0.25,
                    i, position, index);
                ck_assert_int_eq(index[0], 0);

                turtle_stepper_step(stepper, position, NULL, NULL, NULL,
                    &altitude, NULL, NULL, index);
                if (i) {
                        ck_assert_int_eq(index[0], i);
                        ck_assert_int_eq(index[1], 0);
                } else {
                        ck_assert_int_eq(index[0], 0);
                }
                values[0][i] = altitude;

                turtle_stepper_position(stepper, 45.5, 2.5, -0.25, i,
                    position, index);
                ck_assert_int_eq(index[0], 1);

                turtle_stepper_step(stepper, position, NULL, NULL, NULL,
                    &altitude, NULL, NULL, index);
                ck_assert_int_eq(index[0], i);
                ck_assert_int_eq(index[1], 1);
                values[1][i] = altitude;

                turtle_stepper_position(stepper, 40., 10., -0.25, i,
                    position, index);
                ck_assert_int_eq(index[0], 2);

                turtle_stepper_step(stepper, position, NULL, NULL, NULL,
                    &altitude, NULL, NULL, index);
                ck_assert_int_eq(index[0], i);
                ck_assert_int_eq(index[1], 2);
                values[2][i] = altitude;
        }
        ck_assert_int_eq(stepper->data.size, 3);
        ck_assert_int_eq(stepper->transforms.size, 2);
        ck_assert_int_eq(stepper->layers.size, 2);

        /* Check that the elevation values are consistent */
        for (i = 0; i < 3; i++) {
                ck_assert_double_eq_tol(values[i][0] - offset[0],
                    values[i][1] - offset[1], FLT_EPSILON);
        }

        /* Check the initial stepping for a top position */
        const double slope = turtle_stepper_slope_get(stepper); 
        double position[3], altitude, elevation[2], step;
        int index[2];
        turtle_stepper_position(stepper, latitude0, longitude0, 0.5, 1,
            position, index);
        ck_assert_int_eq(index[0], 0);
        turtle_stepper_step(stepper, position, NULL, NULL, NULL,
                &altitude, elevation, &step, index);
        ck_assert_int_eq(index[0], 2);
        ck_assert_double_eq_tol(
            altitude - 0.5, values[0][1] + 0.25, FLT_EPSILON);
        ck_assert_double_eq_tol(
            elevation[0], values[0][1] + 0.25, FLT_EPSILON);
        ck_assert_double_eq(elevation[1], DBL_MAX);
        ck_assert_double_eq_tol(step, 0.5 * slope, FLT_EPSILON);

        /* Check the initial stepping for a middle position */
        turtle_stepper_position(stepper, latitude0, longitude0, -0.1, 1,
            position, index);
        ck_assert_int_eq(index[0], 0);
        turtle_stepper_step(stepper, position, NULL, NULL, NULL,
                &altitude, elevation, &step, index);
        ck_assert_int_eq(index[0], 1);
        ck_assert_int_eq(index[1], 0);
        ck_assert_double_eq_tol(
            altitude + 0.1, values[0][1] + 0.25, FLT_EPSILON);
        ck_assert_double_eq_tol(
            elevation[0], values[0][0] + 0.25, FLT_EPSILON);
        ck_assert_double_eq_tol(
            elevation[1], values[0][1] + 0.25, FLT_EPSILON);
        ck_assert_double_eq_tol(step, 0.1 * slope, FLT_EPSILON);

        /* Check the initial stepping for a bottom position */
        turtle_stepper_position(stepper, latitude0, longitude0, -0.5, 0,
            position, index);
        ck_assert_int_eq(index[0], 0);
        turtle_stepper_step(stepper, position, NULL, NULL, NULL,
                &altitude, elevation, &step, index);
        ck_assert_int_eq(index[0], 0);
        ck_assert_int_eq(index[1], 0);
        ck_assert_double_eq_tol(
            altitude + 0.5, values[0][0] + 0.25, FLT_EPSILON);
        ck_assert_double_eq_tol(
            elevation[1], values[0][0] + 0.25, FLT_EPSILON);
        ck_assert_double_eq(elevation[0], -DBL_MAX);
        ck_assert_double_eq_tol(step, 0.5 * slope, FLT_EPSILON);

        /* Check the stepping when intersecting a layer */
        double direction[3];
        turtle_stepper_position(stepper, latitude0, longitude0, -0.1, 1,
            position, index);
        turtle_ecef_from_horizontal(latitude0, longitude0, 0., 90., direction);
        turtle_stepper_slope_set(stepper, 2.);
        turtle_stepper_step(stepper, position, direction, NULL, NULL,
                &altitude, elevation, &step, index);
        ck_assert_int_eq(index[0], 2);
        ck_assert_int_eq(index[1], 0);
        ck_assert_double_eq_tol(altitude, values[0][1] + 0.25, 1E-05);
        ck_assert_double_eq_tol(step, 0.1, 1E-05);

        /* Check the stepping from the boundary */
        const double resolution = turtle_stepper_resolution_get(stepper);
        turtle_stepper_step(stepper, position, direction, NULL, NULL,
                &altitude, elevation, &step, index);
        ck_assert_int_eq(index[0], 2);
        ck_assert_int_eq(index[1], 0);
        ck_assert_double_eq_tol(
            altitude - resolution, values[0][1] + 0.25, 1E-05);
        ck_assert_double_eq_tol(step, resolution, 1E-05);

        /* Clean and exit */
        turtle_stepper_destroy(&stepper);
        turtle_stack_destroy(&stack);
        turtle_map_destroy(&geoid);
        turtle_map_destroy(&map);
}
END_TEST


START_TEST (test_map)
{
        DEFINE_MAP_META();

        /* Load the map */
        struct turtle_map * map;
        turtle_map_load(&map, MAP_PATH);

        /* Check the map meta data */
        struct turtle_map_info info = { 0 };
        const char * projection;
        turtle_map_meta(map, &info, &projection);

        ck_assert_int_eq(info.nx, nx);
        ck_assert_int_eq(info.ny, ny);
        ck_assert_double_eq(info.x[0], x0 - 1000);
        ck_assert_double_eq(info.x[1], x0 + 1000);
        ck_assert_double_eq(info.y[0], y0 - 1000);
        ck_assert_double_eq(info.y[1], y0 + 1000);
        ck_assert_double_eq(info.z[0], z0);
        ck_assert_double_eq(info.z[1], z1);
        ck_assert_str_eq(projection, "UTM 31N");

        /* Check the access to the map data */
        double z;
        int i, k;
        for (i = 0, k = 0; i < nx; i++) {
                int j;
                for (j = 0; j < ny; j++, k++) {
                        turtle_map_node(map, i, j, NULL, NULL, &z);
                        const double zz = ((k % 2) == 0) ? z0 : z1;
                        ck_assert_double_eq(z, zz);
                }
        }

        /* Check the interpolation */
        turtle_map_elevation(map, x0, y0, &z, NULL);
        turtle_map_elevation(map, x0 + 0.5, y0 + 0.5, &z, NULL);
        int inside;
        turtle_map_elevation(map, x0 - 1000.5, y0, &z, &inside);
        ck_assert_int_eq(inside, 0);

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
        turtle_map_destroy(&map);

        /* Catch errors and try loading some wrong maps */
        turtle_error_handler_t * handler = turtle_error_handler_get();
        turtle_error_handler_set(&catch_error);

        regex_t regex;
        enum turtle_return rc;

        rc = turtle_map_load(&map, "nothing");
        ck_assert_int_eq(rc, TURTLE_RETURN_BAD_EXTENSION);
        regcomp(&regex, "{ turtle_map_load \\[#[0-9]*\\], "
            "src/turtle/io.c:[0-9]* } no valid format for file `nothing'", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0);
        regfree(&regex);

        rc = turtle_map_load(&map, "nothing.png");
        ck_assert_int_eq(rc, TURTLE_RETURN_PATH_ERROR);
        regcomp(&regex, "{ turtle_map_load \\[#[0-9]*\\], "
            "src/turtle/io/png16.c:[0-9]* } could not open file `nothing.png'",
            0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0); 
        regfree(&regex);

        rc = turtle_map_create(&map, &info, "nothing");
        ck_assert_int_eq(rc, TURTLE_RETURN_BAD_PROJECTION);
        regcomp(&regex, "{ turtle_map_create \\[#[0-9]*\\], "
            "src/turtle/projection.c:[0-9]* } invalid projection `nothing'", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0); 
        regfree(&regex);

        info.nx = 0;
        rc = turtle_map_create(&map, &info, projection);
        ck_assert_int_eq(rc, TURTLE_RETURN_DOMAIN_ERROR);
        regcomp(&regex, "{ turtle_map_create \\[#[0-9]*\\], "
            "src/turtle/map.c:[0-9]* } invalid input parameter(s)", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0); 
        regfree(&regex);

        /* Restore the error handler */
        turtle_error_handler_set(handler);
}
END_TEST


START_TEST (test_projection)
{
        /* Check the no projection case */
        struct turtle_map * map;
        struct turtle_map_info info = { 11, 11, { 45, 46 }, { 3, 4 },
                { -1, 1 } };
        turtle_map_create(&map, &info, NULL);
        ck_assert_ptr_eq(turtle_map_projection(map), NULL);
        turtle_map_destroy(&map);

        /* Load the UTM projection from the map */
        turtle_map_destroy(&map);
        turtle_map_load(&map, MAP_PATH);
        const struct turtle_projection * utm = turtle_map_projection(map);
        ck_assert_str_eq(turtle_projection_name(utm), "UTM 31N");

        /* Create a new empty projection */
        struct turtle_projection * projection;
        turtle_projection_create(&projection, NULL);
        ck_assert_ptr_eq(turtle_projection_name(projection), NULL);

        /* Loop over known projections */
        const char * tag[] = { "Lambert I", "Lambert II", "Lambert IIe",
                "Lambert III", "Lambert IV", "Lambert 93", "UTM 31N",
                "UTM 3.0N", "UTM 31S", "UTM 3.0S" };

        int i;
        for (i = 0; i < sizeof(tag) / sizeof(*tag); i++) {
                /* Reconfigure the projection object */
                turtle_projection_configure(projection, tag[i]);
                ck_assert_str_eq(turtle_projection_name(projection), tag[i]);

                /* Project for and back */
                const double latitude = 45.5, longitude = 3.5;
                double x, y, la, lo;
                turtle_projection_project(
                    projection, latitude, longitude, &x, &y);
                turtle_projection_unproject(projection, x, y, &la, &lo);
                ck_assert_double_eq_tol(la, latitude, 1E-08);
                ck_assert_double_eq_tol(lo, longitude, 1E-08);
        }

        /* Clean the memory */
        turtle_map_destroy(&map);
        turtle_projection_destroy(&projection);

        /* Catch errors and try creating some wrong projection */
        turtle_error_handler_t * handler = turtle_error_handler_get();
        turtle_error_handler_set(&catch_error);

        enum turtle_return rc = turtle_projection_create(
            &projection, "nothing");
        ck_assert_int_eq(rc, TURTLE_RETURN_BAD_PROJECTION);
        regex_t regex;
        regcomp(&regex, "{ turtle_projection_create "
            "\\[#[0-9]*\\], src/turtle/projection.c:[0-9]* } "
            "invalid projection `nothing'", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0);
        regfree(&regex);

        /* Restore the error handler */
        turtle_error_handler_set(handler);
}
END_TEST


START_TEST (test_ecef)
{
        /* Convert a position for and back */
        const double latitude = 45.5, longitude = 3.5, altitude = 1000.;
        double position[3];
        turtle_ecef_from_geodetic(latitude, longitude, altitude, position);

        double lla[3];
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        ck_assert_double_eq_tol(lla[0], latitude, 1E-08);
        ck_assert_double_eq_tol(lla[1], longitude, 1E-08);
        ck_assert_double_eq_tol(lla[2], altitude, 1E-08);

        /* Convert a direction for and back */
        const double azimuth = 60., elevation = 30.;
        double direction[3];
        turtle_ecef_from_horizontal(
            latitude, longitude, azimuth, elevation, direction);
        double angle[2];
        turtle_ecef_to_horizontal(
            latitude, longitude, direction, angle, angle + 1);
        ck_assert_double_eq_tol(angle[0], azimuth, 1E-08);
        ck_assert_double_eq_tol(angle[1], elevation, 1E-08);

        /* Check the boundary cases */
        turtle_ecef_from_geodetic(90, 0, altitude, position);
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        ck_assert_double_eq(lla[0], 90);
        ck_assert_double_eq(lla[1], 0);
        ck_assert_double_eq(lla[2], altitude);

        turtle_ecef_from_geodetic(-90, 0, altitude, position);
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        ck_assert_double_eq(lla[0], -90);
        ck_assert_double_eq(lla[1], 0);
        ck_assert_double_eq(lla[2], altitude);

        turtle_ecef_from_geodetic(0, 90, altitude, position);
        turtle_ecef_to_geodetic(position, lla, lla + 1, lla + 2);
        ck_assert_double_eq(lla[0], 0);
        ck_assert_double_eq(lla[1], 90);
        ck_assert_double_eq(lla[2], altitude);
}
END_TEST


START_TEST (test_stack)
{
        /* Create the stack */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, STACK_PATH, 3, NULL, NULL);
        ck_assert_int_eq(stack->tiles.size, 0);

        /* Check some elevation values */
        double z;
        turtle_stack_elevation(stack, 45.5, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 1);
        turtle_stack_elevation(stack, 45.0, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 1);
        turtle_stack_elevation(stack, 46.5, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 2);
        turtle_stack_elevation(stack, 45.0, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 2);

        int inside;
        turtle_stack_elevation(stack, 45.5, 4.5, &z, &inside);
        ck_assert_int_eq(inside, 0);
        ck_assert_int_eq(stack->tiles.size, 2);

        turtle_stack_elevation(stack, 45.5, 2.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 3);
        turtle_stack_elevation(stack, 46.5, 2.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 3);

        /* Check the clear and load functions */
        turtle_stack_clear(stack);
        ck_assert_int_eq(stack->tiles.size, 0);

        turtle_stack_elevation(stack, 45.5, 2.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        ck_assert_int_eq(stack->tiles.size, 1);

        turtle_stack_load(stack);
        ck_assert_int_eq(stack->tiles.size, 3);

        turtle_stack_clear(stack);
        ck_assert_int_eq(stack->tiles.size, 0);

        turtle_stack_load(stack);
        ck_assert_int_eq(stack->tiles.size, 3);

        turtle_stack_load(stack);
        ck_assert_int_eq(stack->tiles.size, 3);

        turtle_stack_destroy(&stack);
        turtle_stack_create(&stack, STACK_PATH, 0, NULL, NULL);
        turtle_stack_load(stack);
        ck_assert_int_eq(stack->tiles.size, 4);

        /* Clean the memory */
        turtle_stack_destroy(&stack);
}
END_TEST


/* Dummy lock / unlock emulation */
static int nothing(void) { return 0; }


START_TEST (test_client)
{
        /* Create a stack and its client */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, STACK_PATH, 1, &nothing, &nothing);

        struct turtle_client * client;
        turtle_client_create(&client, stack);

        /* Check some elevation value */
        double z;
        turtle_client_elevation(client, 45.5, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        turtle_client_elevation(client, 45.0, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        turtle_client_elevation(client, 46.5, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        turtle_client_elevation(client, 45.0, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);

        int inside;
        turtle_client_elevation(client, 45.5, 4.5, &z, &inside);
        ck_assert_int_eq(inside, 0);
        turtle_client_elevation(client, 45.5, 4.5, &z, &inside);
        ck_assert_int_eq(inside, 0);

        turtle_client_elevation(client, 45.5, 2.5, &z, NULL);
        ck_assert_double_eq(z, 0);
        turtle_client_elevation(client, 46.5, 2.5, &z, NULL);
        ck_assert_double_eq(z, 0);

        /* Check the clear function */
        turtle_client_clear(client);
        turtle_client_elevation(client, 45.5, 3.5, &z, NULL);
        ck_assert_double_eq(z, 0);

        /* Clean the memory */
        turtle_client_destroy(&client);
        turtle_stack_destroy(&stack);

        /* Catch errors and try some false cases */
        turtle_error_handler_t * handler = turtle_error_handler_get();
        turtle_error_handler_set(&catch_error);

        regex_t regex;
        enum turtle_return rc;

        rc = turtle_client_create(&client, stack);
        ck_assert_int_eq(rc, TURTLE_RETURN_BAD_ADDRESS);
        regcomp(&regex, "{ turtle_client_create \\[#[0-9]*\\], "
            "src/turtle/client.c:[0-9]* } invalid null stack", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0);
        regfree(&regex);

        turtle_stack_create(&stack, STACK_PATH, 0, NULL, NULL);
        rc = turtle_client_create(&client, stack);
        ck_assert_int_eq(rc, TURTLE_RETURN_BAD_ADDRESS);
        regcomp(&regex, "{ turtle_client_create \\[#[0-9]*\\], "
            "src/turtle/client.c:[0-9]* } stack has no lock", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0);
        regfree(&regex);
        turtle_stack_destroy(&stack);

        turtle_stack_create(&stack, STACK_PATH, 1, &nothing, &nothing);
        turtle_client_create(&client, stack);
        rc = turtle_client_elevation(client, 45.5, 4.5, &z, NULL);
        ck_assert_int_eq(rc, TURTLE_RETURN_PATH_ERROR);
        regcomp(&regex, "{ turtle_client_elevation \\[#[0-9]*\\], "
            "src/turtle/stack.c:[0-9]* } missing elevation data in "
            "`" STACK_PATH  "'", 0);
        ck_assert_int_eq(regexec(&regex, error_buffer, 0, NULL, 0), 0);
        regfree(&regex);
        turtle_client_destroy(&client);
        turtle_stack_destroy(&stack);

        /* Restore the error handler */
        turtle_error_handler_set(handler);
}
END_TEST


START_TEST (test_stepper)
{
        /* Create the stepper */
        struct turtle_stack * stack;
        turtle_stack_create(&stack, STACK_PATH, 0, NULL, NULL);
        struct turtle_map * map;
        turtle_map_load(&map, MAP_PATH);
        struct turtle_map * geoid;
        turtle_map_load(&geoid, GEOID_PATH);
        struct turtle_stepper * stepper;
        turtle_stepper_create(&stepper);

        /* Configure the stepper */
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_add_flat(stepper, 10);
        turtle_stepper_add_stack(stepper, stack, 0.);
        turtle_stepper_add_map(stepper, map, 0.);
        turtle_stepper_add_stack(stepper, stack, 0.);
        turtle_stepper_add_map(stepper, map, 0.);

        ck_assert_int_eq(stepper->data.size, 3);
        struct turtle_stepper_layer * current_layer = stepper->layers.tail;
        ck_assert_int_eq(current_layer->meta.size, 5);
        ck_assert_int_eq(stepper->transforms.size, 2);

        int i;
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
                    stepper, latitude, longitude, height, 0, position, &layer);
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
                        ck_assert_double_eq(altitude1, altitude);
                        ck_assert_double_eq(
                            ground_elevation1, ground_elevation);
                        ck_assert_double_eq(la1, la);
                        ck_assert_double_eq(lo1, lo);
                        ck_assert_double_eq(layer1, layer);

                        /* Update the position */
                        turtle_stepper_step(stepper, position, direction, &la,
                            &lo, &altitude, &ground_elevation, NULL, &layer);
                        if (altitude >= altitude_max) break;
                }
        }

        /* Check the case of an outside initial position */
        turtle_stepper_destroy(&stepper);
        turtle_stepper_create(&stepper);
        turtle_stepper_add_map(stepper, map, 0);

        double position[3] = { 1., 2., 3. };
        int layer;
        turtle_stepper_position(stepper, 45., 90., 0., 0, position, &layer);
        ck_assert_double_eq(position[0], 1.);
        ck_assert_double_eq(position[1], 2.);
        ck_assert_double_eq(position[2], 3.);
        ck_assert_int_eq(layer, -1);

        /* Check the case of a single data layer */
        turtle_stepper_destroy(&stepper);
        turtle_stepper_create(&stepper);
        turtle_stepper_add_stack(stepper, stack, 0);

        const double latitude = 45.5, longitude = 2.5, height = -0.5;
        double direction[3];
        turtle_stepper_position(
            stepper, latitude, longitude, height, 0, position, &layer);
        turtle_ecef_from_horizontal(
            latitude, longitude, 0, 0, direction);

        /* Step out of the map */
        const int nmax = 100000;
        for (i = 0; i < nmax; i++) {
                double altitude, ground_elevation, la, lo;
                double step_length;
                int index[2];

                turtle_stepper_step(stepper, position, direction, &la,
                    &lo, &altitude, &ground_elevation, &step_length, index);
                if (index[0] < 0) break;
        }
        ck_assert_int_lt(i, nmax);

        /* Check other geometries */
        turtle_stepper_destroy(&stepper);
        turtle_stepper_create(&stepper);
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_add_layer(stepper);
        turtle_stepper_add_flat(stepper, 0);

        ck_assert_ptr_eq(turtle_stepper_geoid_get(stepper), geoid);
        ck_assert_double_eq(turtle_stepper_range_get(stepper), 1.);
        turtle_stepper_range_set(stepper, 10.);
        ck_assert_double_eq(turtle_stepper_range_get(stepper), 10.);
        ck_assert_double_eq(turtle_stepper_slope_get(stepper), 0.4);
        turtle_stepper_slope_set(stepper, 1.);
        ck_assert_double_eq(turtle_stepper_slope_get(stepper), 1.);
        ck_assert_double_eq(turtle_stepper_resolution_get(stepper), 1E-02);
        turtle_stepper_resolution_set(stepper, 1E-03);
        ck_assert_double_eq(turtle_stepper_resolution_get(stepper), 1E-03);

        turtle_stepper_position(
            stepper, latitude, longitude, height, 0, position, &layer);

        double altitude, ground_elevation[2], la, lo;
        int index[2];
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            ground_elevation, NULL, index);
        ck_assert_double_eq(ground_elevation[0], -DBL_MAX);
        ck_assert_double_eq_tol(ground_elevation[1] + height, altitude, 1E-08);
        ck_assert_int_eq(index[0], 0);
        ck_assert_int_eq(index[1], 0);

        turtle_stepper_destroy(&stepper);
        turtle_stepper_create(&stepper);
        turtle_stepper_add_stack(stepper, stack, 0.);
        turtle_stepper_geoid_set(stepper, geoid);
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            ground_elevation, NULL, index);
        ck_assert_double_eq_tol(ground_elevation[1] + height, altitude, 1E-08);

        turtle_ecef_from_horizontal(latitude, longitude, 0, 0, direction);
        for (i = 0; i < 3; i++) position[i] += direction[i] * 1E+06;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            ground_elevation, NULL, index);
        ck_assert_int_eq(index[0], -1);
        ck_assert_int_eq(index[1], -1);

        turtle_stepper_position(stepper, 80, 0, height, 0, position, &layer);
        ck_assert_int_eq(layer, -1);

        /* Check the client layer */
        turtle_stepper_destroy(&stepper);
        turtle_stack_destroy(&stack);
        turtle_stack_create(&stack, STACK_PATH, 1, &nothing, &nothing);
        turtle_stepper_create(&stepper);
        turtle_stepper_add_stack(stepper, stack, 0.);
        turtle_stepper_range_set(stepper, 100);

        turtle_stepper_position(
            stepper, latitude, longitude, height, 0, position, &layer);
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            ground_elevation, NULL, index);
        ck_assert_double_eq(ground_elevation[0], -DBL_MAX);
        ck_assert_double_eq_tol(ground_elevation[1] + height, altitude, 1E-08);

        double altitude1, ground_elevation1[2], la1, lo1;
        int index1[2];
        turtle_stepper_step(stepper, position, NULL, &la1, &lo1, &altitude1,
            ground_elevation1, NULL, index1);
        ck_assert_double_eq(altitude1, altitude);
        ck_assert_double_eq(ground_elevation1[0], ground_elevation[0]);
        ck_assert_double_eq(ground_elevation1[1], ground_elevation[1]);
        ck_assert_double_eq(la1, la);
        ck_assert_double_eq(lo1, lo);
        ck_assert_int_eq(index1[0], index[0]);
        ck_assert_int_eq(index1[1], index[1]);

        for (i = 0; i < 3; i++) position[i] += direction[i] * 10;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            ground_elevation, NULL, index);
        ck_assert_double_eq(ground_elevation[0], -DBL_MAX);
        ck_assert_double_eq(ground_elevation[1], 0);
        ck_assert_int_eq(index[0], 0);
        ck_assert_int_eq(index[1], 0);

        for (i = 0; i < 3; i++) position[i] += direction[i] * 100;
        turtle_stepper_step(stepper, position, NULL, &la, &lo, &altitude,
            ground_elevation, NULL, index);
        ck_assert_double_eq(ground_elevation[0], -DBL_MAX);
        ck_assert_double_eq(ground_elevation[1], 0);
        ck_assert_int_eq(index[0], 0);
        ck_assert_int_eq(index[1], 0);

        /* Clean the memory */
        turtle_stepper_destroy(&stepper);
        turtle_map_destroy(&geoid);
        turtle_map_destroy(&map);
        turtle_stack_destroy(&stack);
}


#ifndef TURTLE_NO_GRD
START_TEST (test_io_grd)
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
                        if ((k % 170) == 169) fputs("\n", fid);
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
                        ck_assert_double_eq_tol(undulation1, undulation, 1E-02);
                }
        }

        /* Check the writing to a GRD map */
        turtle_map_fill(geoid, 0, 0, 1);
        double undulation;
        turtle_map_elevation(geoid, 0, -90, &undulation, NULL);
        ck_assert_double_eq_tol(undulation, 1, 1E-02);

        /* Clean the memory */
        turtle_map_destroy(&geoid);
}
END_TEST
#endif


#ifndef TURTLE_NO_HGT
START_TEST (test_io_hgt)
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
                        if (((k % 100) == 0) || ((k % 101) == 0)) {
                                const double z1 = ((k % 2) == 0) ? -1 : 1;
                                ck_assert_double_eq_tol(z, z1, 1E-02);
                        }
                }
        }

        /* Check the writing to an HGT map */
        turtle_map_fill(map, 0, 0, 10);
        double z;
        turtle_map_elevation(map, 3, 45, &z, NULL);
        ck_assert_double_eq_tol(z, 10, 1E-02);

        /* Clean the memory */
        turtle_map_destroy(&map);
}
END_TEST
#endif


#ifndef TURTLE_NO_TIFF
START_TEST (test_io_tiff)
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
                        if (((k % 10) == 0) || ((k % 11) == 0)) {
                                const double z1 = ((k % 2) == 0) ? -1 : 1;
                                ck_assert_double_eq(z, z1);
                        }
                }
        }

        /* Check the writing to a GEOTIFF map */
        turtle_map_fill(map, 0, 0, 10);
        double z;
        turtle_map_elevation(map, 3, 45, &z, NULL);
        ck_assert_double_eq_tol(z, 10., 1E-02);

        /* Clean the memory */
        turtle_map_destroy(&map);
}
END_TEST
#endif


#ifndef TURTLE_NO_ASC
START_TEST (test_io_asc)
{
        /* Generate bathymetry data in .asc format, e.g. as used by GEBCO */
        FILE * fid = fopen("tests/bathymetry.asc", "w+");
        fprintf(fid,
            "ncols        10\n"
            "nrows        10\n"
            "xllcorner    142.000000000000\n"
            "yllcorner    35.000000000000\n"
            "cellsize     0.1\n"
            "NODATA_value  9.9692099683868690468e+36\n");

#ifndef M_PI
        /* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif
        const double deg = M_PI / 180.;

        int i, k;
        for (i = 0, k = 0; i < 10; i++) {
                const double latitude = 35.05 + (9 - i) * 0.1;
                const double c = cos(latitude * deg);
                int j;
                for (j = 0; j < 10; j++, k++) {
                        const double longitude = 142.05 + j * 0.1;
                        const double depth =
                            -100 * fabs(c * cos(longitude * deg));
                        if ((k % 8) == 0) fputs(" ", fid);
                        fprintf(fid, " %8.3f", depth);
                        if ((k % 8) == 7) fputs("\n", fid);
                }
        }
        fclose(fid);

        /* Read back the depth using TURTLE */
        struct turtle_map * bathymetry;
        turtle_map_load(&bathymetry, "tests/bathymetry.asc");

        for (i = 0; i < 10; i++) {
                const double latitude = 35.05 + i * 0.1;
                const double c = cos(latitude * deg);
                int j;
                for (j = 0; j < 10; j++) {
                        const double longitude = 142.05 + j * 0.1;
                        const double depth =
                            -100 * fabs(c * cos(longitude * deg));
                        double depth1;
                        int inside;
                        turtle_map_elevation(
                            bathymetry, longitude, latitude, &depth1, &inside);
                        if (inside)
                                ck_assert_double_eq_tol(depth1, depth, 1E-02);
                }
        }

        /* Check the writing to an ASC map */
        turtle_map_fill(bathymetry, 0, 0, -64);
        double depth;
        turtle_map_elevation(bathymetry, 142.05, 35.05, &depth, NULL);
        ck_assert_double_eq_tol(depth, -64, 1E-02);

        /* Clean the memory */
        turtle_map_destroy(&bathymetry);
}
END_TEST
#endif


START_TEST (test_strfunc)
{
#define CHECK_API(FUNCTION)                                                    \
        ck_assert_ptr_nonnull(                                                 \
            turtle_error_function((turtle_function_t *)&FUNCTION))

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
        CHECK_API(turtle_stepper_add_layer);
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

        const char * s =
            turtle_error_function((turtle_function_t *)&nothing);
        ck_assert_pstr_eq(s, NULL);
#undef CHECK_API
}
END_TEST


Suite * create_suite(void)
{
        const int timeout = 60;

        /* The test suite */
        Suite * suite = suite_create("Turtle");

        /* The internals test case */
        TCase * tc_internals = tcase_create("Internals");
        suite_add_tcase(suite, tc_internals);
        tcase_set_timeout(tc_internals, timeout);
        tcase_add_test(tc_internals, test_list);
        tcase_add_test(tc_internals, test_stepper_layer);

        /* The API test case */
        TCase * tc_api = tcase_create("API");
        suite_add_tcase(suite, tc_api);
        tcase_set_timeout(tc_api, timeout);
        tcase_add_test(tc_api, test_map);
        tcase_add_test(tc_api, test_projection);
        tcase_add_test(tc_api, test_ecef);
        tcase_add_test(tc_api, test_stack);
        tcase_add_test(tc_api, test_client);
        tcase_add_test(tc_api, test_stepper);
        tcase_add_test(tc_api, test_strfunc); 

        /* The I/O test case */
        TCase * tc_io = tcase_create("I/O");
        suite_add_tcase(suite, tc_io);
        tcase_set_timeout(tc_io, timeout);
#ifndef TURTLE_NO_GRD
        tcase_add_test(tc_io, test_io_grd);
#endif
#ifndef TURTLE_NO_HGT
        tcase_add_test(tc_io, test_io_hgt);
#endif
#ifndef TURTLE_NO_TIFF
        tcase_add_test(tc_io, test_io_tiff);
#endif
#ifndef TURTLE_NO_ASC
        tcase_add_test(tc_io, test_io_asc);
#endif

        return suite;
}


int main(void)
{
        /* Create the dummy test data */
        setup_data();

        /* Configure the tests amd the runner */
        Suite * suite = create_suite();
        SRunner * runner = srunner_create(suite);
        srunner_set_fork_status(runner, CK_NOFORK);

        /* Run the tests */
        srunner_run_all(runner, CK_NORMAL);
        const int status = srunner_ntests_failed(runner) ?
            EXIT_FAILURE : EXIT_SUCCESS;
        srunner_free(runner);

        /* Return the test status to the OS */
        exit(status);
}

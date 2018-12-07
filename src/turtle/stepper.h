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
 * Turtle ECEF stepper.
 */
#ifndef TURTLE_STEPPER_H
#define TURTLE_STEPPER_H

#include "turtle.h"
#include "turtle/list.h"

struct turtle_stepper_data;
typedef enum turtle_return turtle_stepper_stepper_t(
    struct turtle_stepper * stepper, struct turtle_stepper_data * data,
    const double * position, int has_geodetic, double * geographic,
    double * data_elevation, int * inside);

typedef void turtle_stepper_elevator_t(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double latitude, double longitude,
    double * data_elevation, int * inside);

struct turtle_error_context;
typedef enum turtle_return turtle_stepper_cleaner_t(
    struct turtle_stepper_data * data, struct turtle_error_context * error_);

/* Parameters of a local transform */
struct turtle_stepper_transform {
        struct turtle_list_element element;

        double reference_ecef[3];
        double reference_geographic[5];
        double data[5][3];

        struct {
                int updated;
                double geographic[5];
        } history;

        char name[];
};

struct turtle_stepper_data {
        struct turtle_list_element element;

        turtle_stepper_stepper_t * step;
        turtle_stepper_elevator_t * elevation;
        turtle_stepper_cleaner_t * clean;
        union {
                struct turtle_client * client;
                struct turtle_stack * stack;
                struct turtle_map * map;
        } a;
        struct turtle_stepper_transform * transform;

        struct {
                int updated;
                double geographic[5];
                double elevation;
                int inside;
        } history;
};

struct turtle_stepper_meta {
        struct turtle_list_element element;

        struct turtle_stepper_data * data;
        double offset;
};

struct turtle_stepper_layer {
        struct turtle_list_element element;
        struct turtle_list meta;
};

struct turtle_stepper_sample {
        double position[3];
        double geographic[5];
        double elevation[2];
        int index[2];
};

/* Container for an ECEF stepper */
struct turtle_stepper {
        struct turtle_list data;
        struct turtle_list transforms;
        struct turtle_list layers;
        struct turtle_map * geoid;
        double local_range;
        double slope_factor;
        double resolution_factor;
        struct turtle_stepper_sample last;
};

#endif

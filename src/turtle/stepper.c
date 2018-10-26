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
#include "stepper.h"
#include "client.h"
#include "error.h"
#include "map.h"
#include "projection.h"
#include "stack.h"
#include "turtle.h"
/* C89 standard library */
#include "float.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"

static void ecef_to_geodetic(struct turtle_stepper * stepper,
    const double * position, double * geographic)
{
        turtle_ecef_to_geodetic(
            position, geographic, geographic + 1, geographic + 2);
        if (stepper->geoid != NULL) {
                int inside;
                double undulation;
                const double lo = (geographic[1] >= 0) ? geographic[1]:
                    geographic[1] + 360.;
                turtle_map_elevation(stepper->geoid, lo, geographic[0],
                    &undulation, &inside);
                if (inside) geographic[2] -= undulation;
        }
}

typedef enum turtle_return geographic_computer_t(
    struct turtle_stepper * stepper, struct turtle_stepper_layer * layer,
    const double * position, int n0, double * geographic);

static enum turtle_return compute_geodetic(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position, int n0,
    double * geographic)
{
        ecef_to_geodetic(stepper, position, geographic);
        return TURTLE_RETURN_SUCCESS;
}

static enum turtle_return compute_geomap(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position, int n0,
    double * geographic)
{
        if (n0 == 0) {
                ecef_to_geodetic(stepper, position, geographic);
        }
        struct turtle_map * map = layer->a.map;
        const struct turtle_projection * projection =
            turtle_map_projection(map);
        if (projection != NULL) {
                return turtle_projection_project(projection, geographic[0],
                    geographic[1], geographic + 3, geographic + 4);
        } else {
                geographic[3] = geographic[1];
                geographic[4] = geographic[0];
                return TURTLE_RETURN_SUCCESS;
        }
}

static enum turtle_return get_geographic(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position, int n0,
    int n1, geographic_computer_t * compute_geographic, double * geographic)
{
        if (stepper->local_range <= 0.) {
                /* Local transforms are disabled. Let us do the full
                 * computation
                 */
                return compute_geographic(
                    stepper, layer, position, n0, geographic);
        }

        /* 1st let us compute the local coordinates */
        struct turtle_stepper_transform * transform = &layer->transform;
        double local[3], range = 0.;
        int i;
        for (i = 0; i < 3; i++) {
                double r = position[i] - transform->reference_ecef[i];
                local[i] = r;
                r = fabs(r);
                if (r > range) range = r;
        }

        if (range < stepper->local_range) {
                /* Apply the local transform */
                for (i = n0; i < n1; i++) {
                        geographic[i] = transform->reference_geographic[i];
                        int j;
                        for (j = 0; j < 3; j++)
                                geographic[i] +=
                                    transform->data[i][j] * local[j];
                }
                return TURTLE_RETURN_SUCCESS;
        }

        /* Compute the geographic coordinates */
        enum turtle_return rc =
            compute_geographic(stepper, layer, position, n0, geographic);
        if (rc != TURTLE_RETURN_SUCCESS) return rc;

        /* Let us compute the step length in order to check if it is worth
         * computing a local transform or not.
         */
        double step = 0.;
        for (i = 0; i < 3; i++) {
                const double s = fabs(position[i] - stepper->last.position[i]);
                if (s > step) step = s;
        }

        if (step < 0.33 * stepper->local_range) {
                /* Update the local transform */
                memcpy(transform->reference_ecef, position,
                    sizeof(transform->reference_ecef));
                memcpy(transform->reference_geographic + n0, geographic + n0,
                    (n1 - n0) * sizeof(*transform->reference_geographic));
                for (i = 0; i < 3; i++) {
                        double r[3] = { position[0], position[1], position[2] };
                        r[i] += 10.;
                        double geographic1[5];
                        rc = compute_geographic(
                            stepper, layer, r, n0, geographic1);
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                        int j;
                        for (j = n0; j < n1; j++)
                                transform->data[j][i] =
                                    0.1 * (geographic1[j] - geographic[j]);
                }
        }

        return TURTLE_RETURN_SUCCESS;
}

static enum turtle_return stepper_step_client(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position,
    int has_geodetic, double * geographic, double * ground_elevation,
    int * inside)
{
        *inside = 0;
        if (!has_geodetic) {
                enum turtle_return rc = get_geographic(stepper, layer, position,
                    0, 3, &compute_geodetic, geographic);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }
        return turtle_client_elevation(layer->a.client, geographic[0],
            geographic[1], ground_elevation, inside);
}

static enum turtle_return stepper_step_stack(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position,
    int has_geodetic, double * geographic, double * ground_elevation,
    int * inside)
{
        *inside = 0;
        if (!has_geodetic) {
                enum turtle_return rc = get_geographic(stepper, layer, position,
                    0, 3, &compute_geodetic, geographic);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }
        return turtle_stack_elevation(layer->a.stack, geographic[0],
            geographic[1], ground_elevation, inside);
}

static enum turtle_return stepper_step_map(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position,
    int has_geodetic, double * geographic, double * ground_elevation,
    int * inside)
{
        *inside = 0;
        const int n0 = has_geodetic ? 3 : 0;
        enum turtle_return rc = get_geographic(
            stepper, layer, position, n0, 5, &compute_geomap, geographic);
        if (rc != TURTLE_RETURN_SUCCESS) return rc;
        return turtle_map_elevation(layer->a.map, geographic[3], geographic[4],
            ground_elevation, inside);
}

static enum turtle_return stepper_step_flat(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position,
    int has_geodetic, double * geographic, double * ground_elevation,
    int * inside)
{
        *inside = 1;
        if (!has_geodetic) {
                enum turtle_return rc = get_geographic(stepper, layer, position,
                    0, 3, &compute_geodetic, geographic);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }
        *ground_elevation = layer->a.ground_level;
        return TURTLE_RETURN_SUCCESS;
}

static void stepper_elevation_stack(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, double latitude, double longitude,
    double * ground_elevation, int * inside)
{
        *inside = 0;
        turtle_stack_elevation(
            layer->a.stack, latitude, longitude, ground_elevation, inside);
}

static void stepper_elevation_client(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, double latitude, double longitude,
    double * ground_elevation, int * inside)
{
        *inside = 0;
        turtle_client_elevation(
            layer->a.client, latitude, longitude, ground_elevation, inside);
}

static void stepper_elevation_map(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, double latitude, double longitude,
    double * ground_elevation, int * inside)
{
        const struct turtle_projection * projection =
            turtle_map_projection(layer->a.map);
        if (projection != NULL) {
                double x, y;
                turtle_projection_project(projection, latitude, longitude, &x, &y);
                turtle_map_elevation(layer->a.map, x, y, ground_elevation, inside);
        } else {
               turtle_map_elevation(layer->a.map, longitude, latitude,
                   ground_elevation, inside);
        }
}

static void stepper_elevation_flat(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, double latitude, double longitude,
    double * ground_elevation, int * inside)
{
        *inside = 1;
        *ground_elevation = layer->a.ground_level;
}

static enum turtle_return stepper_clean_client(
    struct turtle_stepper_layer * layer, struct turtle_error_context * error_)
{
        return turtle_client_destroy_(&layer->a.client, error_);
}

static void add_layer(
    struct turtle_stepper * stepper, struct turtle_stepper_layer * layer)
{
        layer->next = NULL;
        layer->previous = stepper->layers;
        if (stepper->layers != NULL) stepper->layers->next = layer;
        stepper->layers = layer;

        layer->transform.reference_ecef[0] = DBL_MAX;
        layer->transform.reference_ecef[1] = DBL_MAX;
        layer->transform.reference_ecef[2] = DBL_MAX;
}

enum turtle_return turtle_stepper_add_stack(
    struct turtle_stepper * stepper, struct turtle_stack * stack)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_stack);

        enum turtle_return rc;
        struct turtle_client * client = NULL;
        if (stack->lock != NULL) {
                /* Get a new client for the stack */
                rc = turtle_client_create(&client, stack);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }

        /* Allocate the layer */
        struct turtle_stepper_layer * layer = malloc(sizeof(*layer));
        if (layer == NULL) {
                if (client != NULL) {
                        rc = turtle_client_destroy(&client);
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                }
                return TURTLE_ERROR_MEMORY();
        }

        if (client != NULL) {
                layer->step = &stepper_step_client;
                layer->elevation = &stepper_elevation_client;
                layer->clean = &stepper_clean_client;
                layer->a.client = client;
        } else {
                layer->step = &stepper_step_stack;
                layer->elevation = &stepper_elevation_stack;
                layer->clean = NULL;
                layer->a.stack = stack;
        }

        /* Add the layer to the stepper's stack */
        add_layer(stepper, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_add_map(
    struct turtle_stepper * stepper, struct turtle_map * map)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_map);

        /* Allocate the layer */
        struct turtle_stepper_layer * layer = malloc(sizeof(*layer));
        if (layer == NULL) return TURTLE_ERROR_MEMORY();
        layer->step = &stepper_step_map;
        layer->elevation = &stepper_elevation_map;
        layer->clean = NULL;
        layer->a.map = map;

        /* Add the layer to the stepper's stack */
        add_layer(stepper, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_add_flat(
    struct turtle_stepper * stepper, double ground_level)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_flat);

        /* Allocate the layer */
        struct turtle_stepper_layer * layer = malloc(sizeof(*layer));
        if (layer == NULL) return TURTLE_ERROR_MEMORY();
        layer->step = &stepper_step_flat;
        layer->elevation = &stepper_elevation_flat;
        layer->clean = NULL;
        layer->a.ground_level = ground_level;

        /* Add the layer to the stepper's stack */
        add_layer(stepper, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_create(struct turtle_stepper ** stepper_)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_create);

        struct turtle_stepper * stepper = malloc(sizeof(*stepper));
        if (stepper == NULL) return TURTLE_ERROR_MEMORY();
        *stepper_ = stepper;
        stepper->layers = NULL;
        stepper->geoid = NULL;
        stepper->local_range = 1.;
        stepper->slope_factor = 0.4;
        stepper->resolution_factor = 1E-02;
        stepper->last.position[0] = DBL_MAX;
        stepper->last.position[1] = DBL_MAX;
        stepper->last.position[2] = DBL_MAX;

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_destroy(struct turtle_stepper ** stepper)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_destroy);
        if ((stepper == NULL) || (*stepper == NULL))
                return TURTLE_RETURN_SUCCESS;

        struct turtle_stepper_layer * layer = (*stepper)->layers;
        while (layer != NULL) {
                if (layer->clean != NULL) {
                        enum turtle_return rc = layer->clean(layer, error_);
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                }
                struct turtle_stepper_layer * previous = layer->previous;
                free(layer);
                layer = previous;
        }
        free(*stepper);
        *stepper = NULL;

        return TURTLE_RETURN_SUCCESS;
}

static void reset_history(struct turtle_stepper * stepper)
{
        stepper->last.position[0] = DBL_MAX;
        stepper->last.position[1] = DBL_MAX;
        stepper->last.position[2] = DBL_MAX;

        struct turtle_stepper_layer * layer;
        for (layer = stepper->layers; layer != NULL; layer = layer->next) {
                layer->transform.reference_ecef[0] = DBL_MAX;
                layer->transform.reference_ecef[1] = DBL_MAX;
                layer->transform.reference_ecef[2] = DBL_MAX;
        }
}

void turtle_stepper_geoid_set(
    struct turtle_stepper * stepper, struct turtle_map * geoid)
{
        /* Set the geoid model */
        stepper->geoid = geoid;

        /* Reset the stepping history */
        reset_history(stepper);
}

struct turtle_map * turtle_stepper_geoid_get(
    const struct turtle_stepper * stepper)
{
        return stepper->geoid;
}

double turtle_stepper_range_get(const struct turtle_stepper * stepper)
{
        return stepper->local_range;
}

void turtle_stepper_range_set(struct turtle_stepper * stepper, double range)
{
        /* Set the range */
        stepper->local_range = range;

        /* Reset the stepping history */
        reset_history(stepper);
}

double turtle_stepper_slope_get(const struct turtle_stepper * stepper)
{
        return stepper->slope_factor;
}

void turtle_stepper_slope_set(struct turtle_stepper * stepper, double slope)
{
        stepper->slope_factor = slope;
}

double turtle_stepper_resolution_get(const struct turtle_stepper * stepper)
{
        return stepper->resolution_factor;
}

void turtle_stepper_resolution_set(
    struct turtle_stepper * stepper, double resolution)
{
        stepper->resolution_factor = resolution;
}

static enum turtle_return stepper_sample(struct turtle_stepper * stepper,
    const double * position, struct turtle_stepper_sample * sample,
    int check_bounds, struct turtle_error_context * error_)
{
        /* 1st let us check the history */
        if ((position[0] != stepper->last.position[0]) ||
            (position[1] != stepper->last.position[1]) ||
            (position[2] != stepper->last.position[2])) {
                /* Loop over layers and locate the proper data */
                sample->layer = -1;
                int depth, has_geodetic = 0;
                struct turtle_stepper_layer * layer;
                for (layer = stepper->layers, depth = 0; layer != NULL;
                    layer = layer->previous, depth++) {
                        int inside;
                        enum turtle_return rc = layer->step(stepper, layer,
                            position, has_geodetic, sample->geographic,
                            &sample->ground_elevation, &inside);
                        if (sample == &stepper->last) {
                                memcpy(stepper->last.position, position,
                                    sizeof(stepper->last.position));
                        }
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                        if (inside) {
                                sample->layer = depth;
                                return TURTLE_RETURN_SUCCESS;
                        }
                        has_geodetic = 1;
                }
        } else {
                if (sample != &stepper->last)
                        memcpy(sample, &stepper->last, sizeof(*sample));
                return TURTLE_RETURN_SUCCESS;
        }

        if ((sample->layer < 0) && !check_bounds) {
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid layer");
        }
        return TURTLE_RETURN_SUCCESS;
}

static void sample_publish(struct turtle_stepper * stepper, double * latitude,
    double * longitude, double * altitude, double * ground_elevation,
    int * layer_depth)
{
        if (latitude != NULL) *latitude = stepper->last.geographic[0];
        if (longitude != NULL) *longitude = stepper->last.geographic[1];
        if (altitude != NULL) *altitude = stepper->last.geographic[2];
        if (ground_elevation != NULL) {
                if (stepper->last.layer >= 0)
                        *ground_elevation = stepper->last.ground_elevation;
                else
                        *ground_elevation = 0.;
        }
        if (layer_depth != NULL) *layer_depth = stepper->last.layer;
}

enum turtle_return turtle_stepper_step(struct turtle_stepper * stepper,
    double * position, const double * direction, double * latitude,
    double * longitude, double * altitude, double * ground_elevation,
    double * step_length, int * layer_depth)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_step);

        /* Compute the initial geodetic coordinates, or fetch the last ones */
        if (stepper_sample(stepper, position, &stepper->last,
            layer_depth != NULL, error_) != TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();
        if (stepper->last.layer < 0) {
                sample_publish(stepper, latitude, longitude, altitude,
                    ground_elevation, layer_depth);
                if (step_length != NULL) *step_length = 0;
                return TURTLE_RETURN_SUCCESS;
        }

        /* Compute the step length */
        double ds;
        ds = stepper->slope_factor * fabs(stepper->last.geographic[2] -
            stepper->last.ground_elevation);
        if (ds < stepper->resolution_factor) ds = stepper->resolution_factor;

        /* Return the results if no stepping is requested */
        if (direction == NULL) {
                sample_publish(stepper, latitude, longitude, altitude,
                    ground_elevation, layer_depth);
                if (step_length != NULL) *step_length = ds;
                return TURTLE_RETURN_SUCCESS;
        }

        /* Do the tentative step */
        int i;
        for (i = 0; i < 3; i++) position[i] += direction[i] * ds;

        const int inside0 = stepper->last.geographic[2] <
            stepper->last.ground_elevation;
        if (stepper_sample(stepper, position, &stepper->last, 1, error_) !=
            TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();
        const int inside1 = (stepper->last.layer >= 0) ?
            (stepper->last.geographic[2] < stepper->last.ground_elevation) :
            inside0;

        if ((inside0 != inside1) || (stepper->last.layer < 0)) {
                /* A change of medium occured. Let us locate the
                 * change of medium by dichotomy.
                 */
                double ds0 = -ds, ds1 = 0.;
                struct turtle_stepper_sample sample2;
                memcpy(&sample2, &stepper->last, sizeof(sample2));
                while (ds1 - ds0 > 1E-08) {
                        const double ds2 = 0.5 * (ds0 + ds1);
                        double position2[3] = {
                                position[0] + direction[0] * ds2,
                                position[1] + direction[1] * ds2,
                                position[2] + direction[2] * ds2 };
                        if (stepper_sample(stepper, position2, &sample2, 1,
                            error_) != TURTLE_RETURN_SUCCESS)
                                return TURTLE_ERROR_RAISE();
                        const int inside2 = (sample2.layer >= 0) ?
                            (sample2.geographic[2] <
                             sample2.ground_elevation) : inside0;
                        if ((inside2 == inside1) || (sample2.layer < 0)) {
                                ds1 = ds2;
                                memcpy(sample2.position, position2,
                                    sizeof(sample2.position));
                                memcpy(&stepper->last, &sample2,
                                    sizeof(stepper->last));
                        } else
                                ds0 = ds2;
                        }
                ds += ds1;
                for (i = 0; i < 3; i++)
                        position[i] += direction[i] * ds1;
        }

        sample_publish(stepper, latitude, longitude, altitude,
            ground_elevation, layer_depth);
        if (step_length != NULL) *step_length = ds;

        if ((stepper->last.layer < 0) && (layer_depth == NULL)) {
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid layer");
        }
        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_position(struct turtle_stepper * stepper,
    double latitude, double longitude, double height, double * position,
    int * layer_depth)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_position);

        /* Loop over layers and locate the proper data */
        int depth;
        struct turtle_stepper_layer * layer;
        double ground = 0.;
        for (layer = stepper->layers, depth = 0; layer != NULL;
             layer = layer->previous, depth++) {
                int inside;
                layer->elevation(
                    stepper, layer, latitude, longitude, &ground, &inside);
                if (inside) {
                        if (stepper->geoid != NULL) {
                                /* Correct from the geoid */
                                int inside_;
                                double undulation;
                                const double lo = (longitude >= 0) ?
                                    longitude : longitude + 360.;
                                turtle_map_elevation(stepper->geoid, lo,
                                    latitude, &undulation, &inside_);
                                if (inside_) ground += undulation;
                        }

                        /* Compute the ECEF position */
                        turtle_ecef_from_geodetic(
                            latitude, longitude, ground + height, position);
                        if (layer_depth != NULL) *layer_depth = depth;
                        return TURTLE_RETURN_SUCCESS;
                }
        }

        if (layer_depth != NULL) {
                *layer_depth = -1;
                return TURTLE_RETURN_SUCCESS;
        } else {
                return TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid layer");
        }
}

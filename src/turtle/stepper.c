/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for Rendering The eLEvation (TURTLE)
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
#include "datum.h"
#include "error.h"
#include "map.h"
#include "projection.h"
#include "turtle.h"
/* C89 standard library */
#include "float.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"

typedef enum turtle_return geographic_computer_t(
    struct turtle_stepper_layer * layer, const double * position, int n0,
    double * geographic);

enum turtle_return compute_geodetic(struct turtle_stepper_layer * layer,
    const double * position, int n0, double * geographic)
{
        turtle_datum_geodetic(
            NULL, position, geographic, geographic + 1, geographic + 2);
        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return compute_geomap(struct turtle_stepper_layer * layer,
    const double * position, int n0, double * geographic)
{
        if (n0 == 0) {
                turtle_datum_geodetic(
                    NULL, position, geographic, geographic + 1, geographic + 2);
        }
        struct turtle_map * map = layer->a.map;
        struct turtle_projection * projection = turtle_map_projection(map);
        return turtle_projection_project(projection, geographic[0],
            geographic[1], geographic + 3, geographic + 4);
}

static enum turtle_return get_geographic(struct turtle_stepper * stepper,
    struct turtle_stepper_layer * layer, const double * position, int n0,
    int n1, geographic_computer_t * compute_geographic, double * geographic)
{
        if (stepper->local_range <= 0.) {
                /* Local transforms are disabled. Let us do the full
                 * computation
                 */
                return compute_geographic(layer, position, n0, geographic);
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
            compute_geographic(layer, position, n0, geographic);
        if (rc != TURTLE_RETURN_SUCCESS) return rc;

        /* Let us compute the step length in order to check if it is worth
         * computing a local transform or not.
         */
        double step = 0.;
        for (i = 0; i < 3; i++) {
                const double s = fabs(position[i] - stepper->last_position[i]);
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
                        rc = compute_geographic(layer, r, n0, geographic1);
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

static enum turtle_return stepper_step_datum(struct turtle_stepper * stepper,
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
        return turtle_datum_elevation(layer->a.datum, geographic[0],
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

static enum turtle_return stepper_clean_client(
    struct turtle_stepper_layer * layer)
{
        return turtle_client_destroy(&layer->a.client);
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

enum turtle_return turtle_stepper_add_datum(
    struct turtle_stepper * stepper, struct turtle_datum * datum)
{
        enum turtle_return rc;
        struct turtle_client * client = NULL;
        if (datum->lock != NULL) {
                /* Get a new client for the datum */
                rc = turtle_client_create(datum, &client);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }

        /* Allocate the layer */
        struct turtle_stepper_layer * layer = malloc(sizeof(*layer));
        if (layer == NULL) {
                if (client != NULL) {
                        rc = turtle_client_destroy(&client);
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                }
                return TURTLE_ERROR_MEMORY(turtle_stepper_add_datum);
        }
        if (client != NULL) {
                layer->step = &stepper_step_client;
                layer->clean = &stepper_clean_client;
                layer->a.client = client;
        } else {
                layer->step = &stepper_step_datum;
                layer->clean = NULL;
                layer->a.datum = datum;
        }

        /* Add the layer to the stepper's stack */
        add_layer(stepper, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_add_map(
    struct turtle_stepper * stepper, struct turtle_map * map)
{
        /* Allocate the layer */
        struct turtle_stepper_layer * layer = malloc(sizeof(*layer));
        if (layer == NULL) return TURTLE_ERROR_MEMORY(turtle_stepper_add_map);
        layer->step = &stepper_step_map;
        layer->clean = NULL;
        layer->a.map = map;

        /* Add the layer to the stepper's stack */
        add_layer(stepper, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_add_flat(
    struct turtle_stepper * stepper, double ground_level)
{
        /* Allocate the layer */
        struct turtle_stepper_layer * layer = malloc(sizeof(*layer));
        if (layer == NULL) return TURTLE_ERROR_MEMORY(turtle_stepper_add_flat);
        layer->step = &stepper_step_flat;
        layer->clean = NULL;
        layer->a.ground_level = ground_level;

        /* Add the layer to the stepper's stack */
        add_layer(stepper, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_create(struct turtle_stepper ** stepper_)
{
        struct turtle_stepper * stepper = malloc(sizeof(*stepper));
        if (stepper == NULL) return TURTLE_ERROR_MEMORY(turtle_stepper_create);
        *stepper_ = stepper;
        stepper->layers = NULL;
        stepper->local_range = 0.;
        stepper->last_position[0] = DBL_MAX;
        stepper->last_position[1] = DBL_MAX;
        stepper->last_position[2] = DBL_MAX;

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_destroy(struct turtle_stepper ** stepper)
{
        if ((stepper == NULL) || (*stepper == NULL))
                return TURTLE_RETURN_SUCCESS;

        struct turtle_stepper_layer * layer = (*stepper)->layers;
        while (layer != NULL) {
                if (layer->clean != NULL) {
                        enum turtle_return rc = layer->clean(layer);
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

void turtle_stepper_range_set(struct turtle_stepper * stepper, double range)
{
        stepper->local_range = range;
}

double turtle_stepper_range_get(const struct turtle_stepper * stepper)
{
        return stepper->local_range;
}

static void update_(struct turtle_stepper * stepper, const double * position,
    double * geographic, double ground_elevation, int layer)
{
        memcpy(
            stepper->last_position, position, sizeof(stepper->last_position));
        memcpy(stepper->last_geographic, geographic,
            sizeof(stepper->last_geographic));
        stepper->last_ground = ground_elevation;
        stepper->last_layer = layer;
}

enum turtle_return turtle_stepper_step(struct turtle_stepper * stepper,
    const double * position, double * latitude, double * longitude,
    double * altitude, double * ground_elevation, int * layer_depth)
{
        /* 1st let us check the history */
        if ((position[0] == stepper->last_position[0]) &&
            (position[1] == stepper->last_position[1]) &&
            (position[2] == stepper->last_position[2])) {
                if (latitude != NULL) *latitude = stepper->last_geographic[0];
                if (longitude != NULL) *longitude = stepper->last_geographic[1];
                if (altitude != NULL) *altitude = stepper->last_geographic[2];
                if (ground_elevation != NULL)
                        *ground_elevation = stepper->last_ground;

                if (layer_depth != NULL) {
                        *layer_depth = stepper->last_layer;
                } else if (stepper->last_layer < 0) {
                        return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_DOMAIN_ERROR,
                            turtle_stepper_step, "no valid layer");
                }
                return TURTLE_RETURN_SUCCESS;
        }

        /* Loop over layers and locate the proper data */
        if (layer_depth != NULL) *layer_depth = -1;
        int depth, has_geodetic = 0;
        struct turtle_stepper_layer * layer;
        double geographic[5], ground = 0.;
        for (layer = stepper->layers, depth = 0; layer != NULL;
             layer = layer->previous, depth++) {
                int inside;
                enum turtle_return rc = layer->step(stepper, layer, position,
                    has_geodetic, geographic, &ground, &inside);
                update_(stepper, position, geographic, ground, depth);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
                if (inside) {
                        if (latitude != NULL) *latitude = geographic[0];
                        if (longitude != NULL) *longitude = geographic[1];
                        if (altitude != NULL) *altitude = geographic[2];
                        if (ground_elevation != NULL)
                                *ground_elevation = ground;
                        if (layer_depth != NULL) *layer_depth = depth;
                        return TURTLE_RETURN_SUCCESS;
                }
                has_geodetic = 1;
        }

        /* Update and return */
        if (latitude != NULL) *latitude = geographic[0];
        if (longitude != NULL) *longitude = geographic[1];
        if (altitude != NULL) *altitude = geographic[2];
        update_(stepper, position, geographic, ground, depth);
        if (layer_depth != NULL) {
                return TURTLE_RETURN_SUCCESS;
        } else {
                return TURTLE_ERROR_MESSAGE(TURTLE_RETURN_DOMAIN_ERROR,
                    turtle_stepper_step, "no valid layer");
        }
}

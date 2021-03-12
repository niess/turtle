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
    struct turtle_stepper * stepper, struct turtle_stepper_data * data,
    const double * position, int n0, double * geographic);

static enum turtle_return compute_geodetic(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position, int n0,
    double * geographic)
{
        ecef_to_geodetic(stepper, position, geographic);
        return TURTLE_RETURN_SUCCESS;
}

static enum turtle_return compute_geomap(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position, int n0,
    double * geographic)
{
        if (n0 == 0) {
                ecef_to_geodetic(stepper, position, geographic);
        }
        struct turtle_map * map = data->a.map;
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
    struct turtle_stepper_data * data, const double * position, int n0,
    int n1, geographic_computer_t * compute_geographic, double * geographic)
{
        /* Let us check if this transform has already been processed */
        struct turtle_stepper_transform * transform = data->transform;
        if (transform->history.updated) {
                memcpy(geographic + n0, transform->history.geographic + n0,
                    (n1 - n0) * sizeof(double));
                return TURTLE_RETURN_SUCCESS;
        }

        if (stepper->local_range <= 0.) {
                /* Local transforms are disabled. Let us do the full
                 * computation
                 */
                enum turtle_return rc = compute_geographic(
                    stepper, data, position, n0, geographic);
                if (rc != TURTLE_RETURN_SUCCESS)
                        return rc;
                goto backup_and_exit;
        }

        /* 1st let us compute the local coordinates */
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
                goto backup_and_exit;
        }

        /* Compute the geographic coordinates */
        enum turtle_return rc =
            compute_geographic(stepper, data, position, n0, geographic);
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
                            stepper, data, r, n0, geographic1);
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                        int j;
                        for (j = n0; j < n1; j++)
                                transform->data[j][i] =
                                    0.1 * (geographic1[j] - geographic[j]);
                }
        }

        /* Backup the transform result in case that it is called again */
backup_and_exit:
        memcpy(transform->history.geographic + n0, geographic + n0,
            (n1 - n0) * sizeof(double));
        transform->history.updated = 1;

        return TURTLE_RETURN_SUCCESS;
}

static enum turtle_return stepper_step(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position,
    int has_geodetic, double * geographic, double * elevation, int * inside)
{
        if (data->history.updated) {
                memcpy(geographic, data->history.geographic,
                    sizeof(data->history.geographic));
                *elevation = data->history.elevation;
                *inside = data->history.inside;
        } else {
                enum turtle_return rc;
                rc = data->step(stepper, data, position, has_geodetic,
                    geographic, elevation, inside);
                if (rc != TURTLE_RETURN_SUCCESS)
                        return rc;

                data->history.updated = 1;
                memcpy(data->history.geographic, geographic,
                    sizeof(data->history.geographic));
                data->history.elevation = *elevation;
                data->history.inside = *inside;
        }

        return TURTLE_RETURN_SUCCESS;
}

static enum turtle_return stepper_step_client(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position,
    int has_geodetic, double * geographic, double * elevation, int * inside)
{
        *inside = 0;
        if (!has_geodetic) {
                enum turtle_return rc = get_geographic(stepper, data, position,
                    0, 3, &compute_geodetic, geographic);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }
        return turtle_client_elevation(data->a.client, geographic[0],
            geographic[1], elevation, inside);
}

static enum turtle_return stepper_step_stack(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position,
    int has_geodetic, double * geographic, double * elevation, int * inside)
{
        *inside = 0;
        if (!has_geodetic) {
                enum turtle_return rc = get_geographic(stepper, data, position,
                    0, 3, &compute_geodetic, geographic);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }
        return turtle_stack_elevation(data->a.stack, geographic[0],
            geographic[1], elevation, inside);
}

static enum turtle_return stepper_step_map(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position,
    int has_geodetic, double * geographic, double * elevation, int * inside)
{
        *inside = 0;
        const int n0 = has_geodetic ? 3 : 0;
        enum turtle_return rc = get_geographic(
            stepper, data, position, n0, 5, &compute_geomap, geographic);
        if (rc != TURTLE_RETURN_SUCCESS) return rc;
        return turtle_map_elevation(data->a.map, geographic[3], geographic[4],
            elevation, inside);
}

static enum turtle_return stepper_step_flat(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const double * position,
    int has_geodetic, double * geographic, double * elevation, int * inside)
{
        *inside = 1;
        if (!has_geodetic) {
                enum turtle_return rc = get_geographic(stepper, data, position,
                    0, 3, &compute_geodetic, geographic);
                if (rc != TURTLE_RETURN_SUCCESS) return rc;
        }
        *elevation = 0.;
        return TURTLE_RETURN_SUCCESS;
}

static void stepper_elevation(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double latitude, double longitude,
    double * elevation, int * inside)
{
        if (data->history.updated) {
                *elevation = data->history.elevation;
                *inside = data->history.inside; 
        } else {
                data->elevation(
                    stepper, data, latitude, longitude, elevation, inside);
                data->history.updated = 1;
                data->history.elevation = *elevation;
                data->history.inside = *inside;
        }
}

static void stepper_elevation_stack(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double latitude, double longitude,
    double * elevation, int * inside)
{
        *inside = 0;
        turtle_stack_elevation(
            data->a.stack, latitude, longitude, elevation, inside);
}

static void stepper_elevation_client(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double latitude, double longitude,
    double * elevation, int * inside)
{
        *inside = 0;
        turtle_client_elevation(
            data->a.client, latitude, longitude, elevation, inside);
}

static void stepper_elevation_map(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double latitude, double longitude,
    double * elevation, int * inside)
{
        const struct turtle_projection * projection =
            turtle_map_projection(data->a.map);
        if (projection != NULL) {
                double x, y;
                turtle_projection_project(
                    projection, latitude, longitude, &x, &y);
                turtle_map_elevation(
                    data->a.map, x, y, elevation, inside);
        } else {
               turtle_map_elevation(data->a.map, longitude, latitude,
                   elevation, inside);
        }
}

static void stepper_elevation_flat(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double latitude, double longitude,
    double * elevation, int * inside)
{
        *inside = 1;
        *elevation = 0.;
}

static enum turtle_return stepper_clean_client(
    struct turtle_stepper_data * data, struct turtle_error_context * error_)
{
        return turtle_client_destroy_(&data->a.client, error_);
}

static enum turtle_return add_data(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, const char * name)
{
        /* Look for an existing transform */
        struct turtle_stepper_transform * transform;
        for (transform = stepper->transforms.head; transform != NULL;
            transform = transform->element.next) {
                if (strcmp(transform->name, name) == 0) break;
        }

        if (transform == NULL) {
                /* Create the new transform */
                const int n = strlen(name) + 1;
                transform = malloc(sizeof(*data->transform) + n);
                if (transform == NULL)
                        return TURTLE_RETURN_MEMORY_ERROR;

                transform->reference_ecef[0] = DBL_MAX;
                transform->reference_ecef[1] = DBL_MAX;
                transform->reference_ecef[2] = DBL_MAX;
                memcpy(transform->name, name, n);

                turtle_list_append_(&stepper->transforms, transform);
        }

        /* Append the data to the stack */
        data->transform = transform;
        turtle_list_append_(&stepper->data, data);

        return TURTLE_RETURN_SUCCESS;
}

static enum turtle_return stepper_add_layer(struct turtle_stepper * stepper)
{
        struct turtle_stepper_layer * layer = stepper->layers.tail;
        if ((layer != NULL) && (layer->meta.size == 0))
                return TURTLE_RETURN_SUCCESS;

        layer = malloc(sizeof(*layer));
        if (layer == NULL)
                return TURTLE_RETURN_MEMORY_ERROR;
        memset(&layer->meta, 0x0, sizeof(layer->meta));
        turtle_list_append_(&stepper->layers, layer);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_add_layer(struct turtle_stepper * stepper)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_layer);

        if (stepper_add_layer(stepper) != TURTLE_RETURN_SUCCESS) {
                return TURTLE_ERROR_MEMORY();
        } else {
                return TURTLE_RETURN_SUCCESS;
        }
}

static enum turtle_return add_meta(struct turtle_stepper * stepper,
    struct turtle_stepper_data * data, double offset)
{
        /* Add a new geometry layer if none so far */
        if ((stepper->layers.size == 0) && (stepper_add_layer(stepper) !=
            TURTLE_RETURN_SUCCESS))
                return TURTLE_RETURN_MEMORY_ERROR;

        /* Add meta data to the current layer */
        struct turtle_stepper_meta * meta = malloc(sizeof(*meta));
        if (meta == NULL)
                return TURTLE_RETURN_MEMORY_ERROR;
        meta->data = data;
        meta->offset = offset;

        struct turtle_stepper_layer * layer = stepper->layers.tail;
        turtle_list_append_(&layer->meta, meta);

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_add_stack(
    struct turtle_stepper * stepper, struct turtle_stack * stack, double offset)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_stack);

        /* Look if the data already exist */
        struct turtle_stepper_data * data;
        for (data = stepper->data.head; data != NULL;
            data = data->element.next) {
                if ((stack->lock != NULL) &&
                    (data->clean == &stepper_clean_client) &&
                    (data->a.client->stack == stack))
                        break;
                else if (data->a.stack == stack)
                        break;
        }

        struct turtle_client * client = NULL;
        if (data == NULL) {
                /* Allocate an encapsulation of the new data */
                enum turtle_return rc;
                if (stack->lock != NULL) {
                        /* Get a new client for the stack */
                        rc = turtle_client_create(&client, stack);
                        if (rc != TURTLE_RETURN_SUCCESS) return rc;
                }

                /* Allocate the data container */
                data = malloc(sizeof(*data));
                if (data == NULL) goto memory_error;

                if (client != NULL) {
                        data->step = &stepper_step_client;
                        data->elevation = &stepper_elevation_client;
                        data->clean = &stepper_clean_client;
                        data->a.client = client;
                } else {
                        data->step = &stepper_step_stack;
                        data->elevation = &stepper_elevation_stack;
                        data->clean = NULL;
                        data->a.stack = stack;
                }

                /* Add the data to the stepper's stack */
                if (add_data(stepper, data, "geodetic") !=
                    TURTLE_RETURN_SUCCESS)
                        goto memory_error;
        }

        /* Add the meta data to the current layer */
        if (add_meta(stepper, data, offset) != TURTLE_RETURN_SUCCESS)
                goto memory_error;
        else
                return TURTLE_RETURN_SUCCESS;

memory_error:
        if (client != NULL)
                turtle_client_destroy(&client);
        return TURTLE_ERROR_MEMORY();
}

enum turtle_return turtle_stepper_add_map(struct turtle_stepper * stepper,
    struct turtle_map * map, double offset)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_map);

        /* Look if the data already exist */
        struct turtle_stepper_data * data;
        for (data = stepper->data.head; data != NULL;
            data = data->element.next) {
                if (data->a.map == map) break;
        }

        if (data == NULL) {
                /* Allocate an encapsulation of the new data */
                data = malloc(sizeof(*data));
                if (data == NULL) return TURTLE_ERROR_MEMORY();
                data->step = &stepper_step_map;
                data->elevation = &stepper_elevation_map;
                data->clean = NULL;
                data->a.map = map;

                /* Add the data to the stepper's stack */
                const struct turtle_projection * projection =
                    turtle_map_projection(map);
                const char * name = (projection == NULL) ?
                    "geodetic" : turtle_projection_name(projection);
                if (add_data(stepper, data, name) != TURTLE_RETURN_SUCCESS) {
                        return TURTLE_ERROR_MEMORY();
                }
        }

        /* Add the meta data to the current layer */
        if (add_meta(stepper, data, offset) != TURTLE_RETURN_SUCCESS) {
                return TURTLE_ERROR_MEMORY();
        } else {
                return TURTLE_RETURN_SUCCESS;
        }
}

enum turtle_return turtle_stepper_add_flat(
    struct turtle_stepper * stepper, double offset)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_add_flat);

        /* Look if the data already exist */
        struct turtle_stepper_data * data;
        for (data = stepper->data.head; data != NULL;
            data = data->element.next) {
                if (data->a.map == NULL) break;
        }

        if (data == NULL) {
                /* Allocate an encapsulation of the new data */
                data = malloc(sizeof(*data));
                if (data == NULL) return TURTLE_ERROR_MEMORY();
                data->step = &stepper_step_flat;
                data->elevation = &stepper_elevation_flat;
                data->clean = NULL;
                data->a.map = NULL;

                /* Add the data to the stepper's stack */
                if (add_data(stepper, data, "geodetic") !=
                    TURTLE_RETURN_SUCCESS) {
                        return TURTLE_ERROR_MEMORY();
                }
        }

        /* Add the meta data to the current layer */
        if (add_meta(stepper, data, offset) != TURTLE_RETURN_SUCCESS) {
                return TURTLE_ERROR_MEMORY();
        } else {
                return TURTLE_RETURN_SUCCESS;
        }
}

enum turtle_return turtle_stepper_create(struct turtle_stepper ** stepper_)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_create);

        struct turtle_stepper * stepper = malloc(sizeof(*stepper));
        if (stepper == NULL) return TURTLE_ERROR_MEMORY();
        *stepper_ = stepper;
        memset(&stepper->data, 0x0, sizeof(stepper->data));
        memset(&stepper->transforms, 0x0, sizeof(stepper->transforms));
        memset(&stepper->layers, 0x0, sizeof(stepper->layers));
        stepper->geoid = NULL;
        stepper->local_range = 1.;
        stepper->slope_factor = 0.4;
        stepper->resolution_factor = 1E-02;
        stepper->last.index[0] = -1;
        stepper->last.index[1] = -1;
        stepper->last.elevation[0] = 0;
        stepper->last.elevation[1] = 0;
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

        struct turtle_stepper_data * data;
        while ((data = turtle_list_pop_(&(*stepper)->data)) != NULL) {
                if (data->clean != NULL) {
                        enum turtle_return rc = data->clean(data, error_);
                        if (rc != TURTLE_RETURN_SUCCESS)
                                return TURTLE_ERROR_RAISE();
                }
                free(data);
        }

        turtle_list_clear_(&(*stepper)->transforms);

        struct turtle_stepper_layer * layer;
        while ((layer = turtle_list_pop_(&(*stepper)->layers)) != NULL) {
                turtle_list_clear_(&layer->meta);
                free(layer);
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

        struct turtle_stepper_data * data;
        for (data = stepper->data.head; data != NULL;
             data = data->element.next) {
                data->transform->reference_ecef[0] = DBL_MAX;
                data->transform->reference_ecef[1] = DBL_MAX;
                data->transform->reference_ecef[2] = DBL_MAX;
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

static void reset_data_and_transforms(struct turtle_stepper * stepper)
{
        struct turtle_stepper_transform * transform;
        for (transform = stepper->transforms.head; transform != NULL;
            transform = transform->element.next)
                transform->history.updated = 0;

        struct turtle_stepper_data * data;
        for (data = stepper->data.head; data != NULL;
            data = data->element.next)
                data->history.updated = 0;
}

static int check_layer(struct turtle_stepper * stepper,
    struct turtle_stepper_sample * sample, int index[2], double elevation)
{
        if (elevation >= sample->geographic[2]) {
                sample->index[0] = index[0];
                sample->index[1] = index[1];
                sample->elevation[1] = elevation;
                return EXIT_SUCCESS;
        } else {
                sample->index[0] = index[0] + 1;
                sample->index[1] = index[1];
                sample->elevation[0] = elevation;
                return EXIT_FAILURE;
        }
}

static enum turtle_return stepper_sample(struct turtle_stepper * stepper,
    const double * position, struct turtle_stepper_sample * sample,
    int check_bounds, struct turtle_error_context * error_)
{
        /* 1st let us check the history */
        if ((position[0] != stepper->last.position[0]) ||
            (position[1] != stepper->last.position[1]) ||
            (position[2] != stepper->last.position[2])) {
                /* Loop over data and locate the proper data */
                reset_data_and_transforms(stepper);
                sample->index[0] = -1;
                sample->index[1] = -1;
                sample->elevation[0] = -DBL_MAX;
                sample->elevation[1] = DBL_MAX;
                int index[2], has_geodetic = 0;
                struct turtle_stepper_layer * layer;
                for (layer = stepper->layers.head, index[0] = 0; layer != NULL;
                    layer = layer->element.next, index[0]++) {
                        struct turtle_stepper_meta * meta;
                        for (meta = layer->meta.tail, index[1] = 0;
                            meta != NULL; meta = meta->element.previous,
                            index[1]++) {
                                int inside;
                                double elevation;
                                enum turtle_return rc = stepper_step(
                                    stepper, meta->data, position, has_geodetic,
                                    sample->geographic, &elevation, &inside);
                                if (sample == &stepper->last) {
                                        memcpy(stepper->last.position, position,
                                            sizeof(stepper->last.position));
                                }
                                if (rc != TURTLE_RETURN_SUCCESS) return rc;
                                has_geodetic = 1;
                                if (inside) {
                                        elevation += meta->offset;
                                        if (check_layer(stepper, sample, index,
                                            elevation) == EXIT_SUCCESS)
                                                return TURTLE_RETURN_SUCCESS;
                                        break;
                                }
                        }
                }
        } else {
                if (sample != &stepper->last)
                        memcpy(sample, &stepper->last, sizeof(*sample));
                return TURTLE_RETURN_SUCCESS;
        }

        if ((sample->index[0] < 0) && !check_bounds) {
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid data");
        }
        return TURTLE_RETURN_SUCCESS;
}

static void sample_publish(struct turtle_stepper * stepper, double * latitude,
    double * longitude, double * altitude, double * elevation,
    int * index)
{
        if (latitude != NULL) *latitude = stepper->last.geographic[0];
        if (longitude != NULL) *longitude = stepper->last.geographic[1];
        if (altitude != NULL) *altitude = stepper->last.geographic[2];
        if (elevation != NULL) {
                if (stepper->last.index[0] >= 0) {
                        elevation[0] = stepper->last.elevation[0];
                        elevation[1] = stepper->last.elevation[1];
                } else {
                        elevation[0] = 0.;
                        elevation[1] = 0.;
                }
        }
        if (index != NULL) {
                index[0] = stepper->last.index[0];
                index[1] = stepper->last.index[1];
        }
}

enum turtle_return turtle_stepper_step(struct turtle_stepper * stepper,
    double * position, const double * direction, double * latitude,
    double * longitude, double * altitude, double * elevation,
    double * step_length, int * index)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_step);

        /* Compute the initial geodetic coordinates, or fetch the last ones */
        if (stepper_sample(stepper, position, &stepper->last, index
            != NULL, error_) != TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();
        if (stepper->last.index[0] < 0) {
                sample_publish(stepper, latitude, longitude, altitude,
                    elevation, index);
                if (step_length != NULL) *step_length = 0;
                return TURTLE_RETURN_SUCCESS;
        }

        /* Compute the step length */
        double ds = 0.;
        int i;
        for (i = 0; i < 2; i++) {
                if ((stepper->last.index[0] == 0) && (i == 0))
                        continue;
                else if ((stepper->last.index[0] ==
                    stepper->layers.size) && (i == 1))
                        break;

                const double dsi = fabs(stepper->last.geographic[2] -
                    stepper->last.elevation[i]);
                if ((dsi < ds) || (ds <= 0.)) ds = dsi;
        }
        ds *= stepper->slope_factor;
        if (ds < stepper->resolution_factor) ds = stepper->resolution_factor;

        /* Return the results if no stepping is requested */
        if (direction == NULL) {
                sample_publish(stepper, latitude, longitude, altitude,
                    elevation, index);
                if (step_length != NULL) *step_length = ds;
                return TURTLE_RETURN_SUCCESS;
        }

        /* Do the tentative step */
        for (i = 0; i < 3; i++) position[i] += direction[i] * ds;

        const int medium0 = stepper->last.index[0];
        if (stepper_sample(stepper, position, &stepper->last, 1, error_) !=
            TURTLE_RETURN_SUCCESS)
                return TURTLE_ERROR_RAISE();
        int medium1 = stepper->last.index[0];

        if (medium0 != medium1) {
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
                        const int medium2 = sample2.index[0];
                        if (medium2 == medium0) {
                                ds0 = ds2;
                        } else {
                                medium1 = medium2; /* In case that a 3rd medium
                                                      was hit in between */
                                ds1 = ds2;
                                memcpy(sample2.position, position2,
                                    sizeof(sample2.position));
                                memcpy(&stepper->last, &sample2,
                                    sizeof(stepper->last));
                        }
                }
                ds += ds1;
                for (i = 0; i < 3; i++)
                        position[i] += direction[i] * ds1;
        }

        sample_publish(stepper, latitude, longitude, altitude,
            elevation, index);
        if (step_length != NULL) *step_length = ds;

        if ((stepper->last.index[0] < 0) && (index == NULL)) {
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid data");
        }
        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_stepper_position(struct turtle_stepper * stepper,
    double latitude, double longitude, double height, int layer_index,
    double * position, int * data_index)
{
        TURTLE_ERROR_INITIALISE(&turtle_stepper_position);

        if ((layer_index < 0) || (layer_index >= stepper->layers.size)) {
                return TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid data");
        }

        struct turtle_stepper_layer * layer;
        for (layer = stepper->layers.head; layer_index > 0; layer_index--,
            layer = layer->element.next) ;

        /* Loop over data and locate the proper set */
        reset_data_and_transforms(stepper);
        int index;
        double elevation = 0.;
        struct turtle_stepper_meta * meta;
        for (meta = layer->meta.tail, index = 0; meta != NULL;
            meta = meta->element.previous, index++) {
                int inside;
                stepper_elevation(stepper, meta->data, latitude, longitude,
                    &elevation, &inside);
                if (inside) {
                        elevation += meta->offset;

                        if (stepper->geoid != NULL) {
                                /* Correct from the geoid */
                                int inside_;
                                double undulation;
                                const double lo = (longitude >= 0) ?
                                    longitude : longitude + 360.;
                                turtle_map_elevation(stepper->geoid, lo,
                                    latitude, &undulation, &inside_);
                                if (inside_) elevation += undulation;
                        }

                        /* Compute the ECEF position */
                        turtle_ecef_from_geodetic(
                            latitude, longitude, elevation + height, position);
                        if (data_index != NULL) *data_index = index;
                        return TURTLE_RETURN_SUCCESS;
                }
        }

        if (data_index != NULL) {
                *data_index = -1;
                return TURTLE_RETURN_SUCCESS;
        } else {
                return TURTLE_ERROR_MESSAGE(
                    TURTLE_RETURN_DOMAIN_ERROR, "no valid data");
        }
}

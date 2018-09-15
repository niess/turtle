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

#include "error.h"
/* C89 standard library */
#include <stdio.h>
#include <stdlib.h>

/* Default handler for TURTLE library errors */
static void handle_error(
    enum turtle_return rc, turtle_function_t * function, const char * message)
{
        fprintf(stderr, "A TURTLE library error occurred:\n%s\n", message);
        exit(EXIT_FAILURE);
}

/* The library user supplied error handler */
static turtle_error_handler_t * _handler = &handle_error;

/* Getter for the error handler */
turtle_error_handler_t * turtle_error_handler_get(void) { return _handler; }

/* Setter for the error handler */
void turtle_error_handler_set(turtle_error_handler_t * handler)
{
        _handler = handler;
}

/* Utility function for formating an error */
enum turtle_return turtle_error_format_(struct turtle_error_context * error_,
    enum turtle_return rc, const char * file, int line, const char * format,
    ...)
{
        error_->code = rc;
        if ((_handler == NULL) || (rc == TURTLE_RETURN_SUCCESS)) return rc;

        /* Format the error message */
        const int n = snprintf(error_->message, TURTLE_ERROR_MSG_LENGTH,
            "{ %s [#%d], %s:%d } ", turtle_error_function(error_->function),
            rc, file, line);
        if (n < TURTLE_ERROR_MSG_LENGTH - 1) {
                va_list ap;
                va_start(ap, format);
                vsnprintf(error_->message + n, TURTLE_ERROR_MSG_LENGTH - n,
                    format, ap);
                va_end(ap);
        }

        return rc;
}

/* Utility function for handling an error */
enum turtle_return turtle_error_raise_(struct turtle_error_context * error_)
{
        if ((_handler == NULL) || (error_->code == TURTLE_RETURN_SUCCESS))
                return error_->code;

        _handler(error_->code, error_->function, error_->message);
        return error_->code;
}

/* Get a library function name as a string */
const char * turtle_error_function(turtle_function_t * caller)
{
#define TOSTRING(function)                                                     \
        if (caller == (turtle_function_t *)function) return #function

        TOSTRING(turtle_client_clear);
        TOSTRING(turtle_client_create);
        TOSTRING(turtle_client_destroy);
        TOSTRING(turtle_client_elevation);
        
        TOSTRING(turtle_ecef_from_geodetic);
        TOSTRING(turtle_ecef_from_horizontal);
        TOSTRING(turtle_ecef_to_geodetic);
        TOSTRING(turtle_ecef_to_horizontal);
        
        TOSTRING(turtle_error_function);
        TOSTRING(turtle_error_handler_get);
        TOSTRING(turtle_error_handler_set);
        
        TOSTRING(turtle_map_create);
        TOSTRING(turtle_map_destroy);
        TOSTRING(turtle_map_dump);
        TOSTRING(turtle_map_elevation);
        TOSTRING(turtle_map_fill);
        TOSTRING(turtle_map_load);
        TOSTRING(turtle_map_meta);
        TOSTRING(turtle_map_node);
        TOSTRING(turtle_map_projection);
        
        TOSTRING(turtle_projection_configure);
        TOSTRING(turtle_projection_create);
        TOSTRING(turtle_projection_destroy);
        TOSTRING(turtle_projection_name);
        TOSTRING(turtle_projection_project);
        TOSTRING(turtle_projection_unproject);
        
        TOSTRING(turtle_stack_clear);
        TOSTRING(turtle_stack_create);
        TOSTRING(turtle_stack_destroy);
        TOSTRING(turtle_stack_elevation);
        TOSTRING(turtle_stack_load);
        
        TOSTRING(turtle_stepper_add_flat);
        TOSTRING(turtle_stepper_add_map);
        TOSTRING(turtle_stepper_add_stack);
        TOSTRING(turtle_stepper_create);
        TOSTRING(turtle_stepper_destroy);
        TOSTRING(turtle_stepper_geoid_get);
        TOSTRING(turtle_stepper_geoid_set);
        TOSTRING(turtle_stepper_range_get);
        TOSTRING(turtle_stepper_range_set);
        TOSTRING(turtle_stepper_position);
        TOSTRING(turtle_stepper_step);

        return NULL;
#undef TOSTRING
}

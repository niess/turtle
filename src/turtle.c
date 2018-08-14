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
 * General purpose routines for the Turtle C library.
 */

#include "turtle.h"
#include "turtle/loader/geotiff16.h"

/* Initialise the TURTLE library */
void turtle_initialise(turtle_handler_cb * handler)
{
        /* Set any user supplied error handler. */
        turtle_handler(handler);

#ifndef TURTLE_NO_TIFF
        /* Register the geotiff16 tags. */
        /* DEBUG geotiff16_register(); */
#endif
}

/* Clear the TURTLE library */
void turtle_finalise(void) {}

/* Get a library function name as a string */
const char * turtle_strfunc(turtle_function_t * caller)
{
#define TOSTRING(function)                                                     \
        if (caller == (turtle_function_t *)function) return #function;

        TOSTRING(turtle_projection_create)
        TOSTRING(turtle_projection_configure)
        TOSTRING(turtle_projection_info)
        TOSTRING(turtle_projection_project)
        TOSTRING(turtle_projection_unproject)
        TOSTRING(turtle_map_create)
        TOSTRING(turtle_map_load)
        TOSTRING(turtle_map_dump)
        TOSTRING(turtle_map_fill)
        TOSTRING(turtle_map_node)
        TOSTRING(turtle_map_elevation)
        TOSTRING(turtle_stack_create)
        TOSTRING(turtle_stack_destroy)
        TOSTRING(turtle_stack_clear)
        TOSTRING(turtle_stack_elevation)
        TOSTRING(turtle_client_create)
        TOSTRING(turtle_client_destroy)
        TOSTRING(turtle_client_clear)
        TOSTRING(turtle_client_elevation)
        TOSTRING(turtle_ecef_to_geodetic)
        TOSTRING(turtle_ecef_from_geodetic)
        TOSTRING(turtle_ecef_to_horizontal)
        TOSTRING(turtle_ecef_from_horizontal)

        return NULL;
#undef TOSTRING
}

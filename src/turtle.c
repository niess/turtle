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
#include "geotiff16.h"
#include "turtle_return.h"

/* The library user supplied error handler. */
static turtle_handler_cb * _handler = NULL;

/* Initialise the TURTLE interface. */
void turtle_initialise(turtle_handler_cb * handler)
{
        /* Set any user supplied error handler. */
        turtle_handler(handler);

#ifndef TURTLE_NO_TIFF
        /* Register the geotiff16 tags. */
        geotiff16_register();
#endif
}

/* Clear the TURTLE interface.
 */
void turtle_finalise(void) {}

/* Get a return code as a string. */
const char * turtle_strerror(enum turtle_return rc)
{
        static const char * msg[N_TURTLE_RETURNS] = { "Operation succeeded",
                "Bad address", "Bad file extension", "Bad file format",
                "Unknown projection", "Bad JSON header",
                "Value is out of bound", "An internal error occured",
                "Couldn't lock", "Not enough memory",
                "No such file or directory", "Couldn't unlock" };

        if ((rc < 0) || (rc >= N_TURTLE_RETURNS))
                return NULL;
        else
                return msg[rc];
}

/* Get a library function name as a string. */
const char * turtle_strfunc(turtle_caller_t * caller)
{
#define TOSTRING(function)                                                     \
        if (caller == (turtle_caller_t *)function) return #function;

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
        TOSTRING(turtle_datum_create)
        TOSTRING(turtle_datum_clear)
        TOSTRING(turtle_datum_direction)
        TOSTRING(turtle_datum_elevation)
        TOSTRING(turtle_datum_ecef)
        TOSTRING(turtle_datum_geodetic)
        TOSTRING(turtle_client_create)
        TOSTRING(turtle_client_destroy)
        TOSTRING(turtle_client_clear)
        TOSTRING(turtle_client_elevation)

        return NULL;
#undef TOSTRING
}

/* Setter for the error handler. */
void turtle_handler(turtle_handler_cb * handler) { _handler = handler; }

/* Utility function for encapsulating `returns` with the error handler. */
enum turtle_return turtle_return(enum turtle_return rc, void (*caller)(void))
{
        if ((_handler != NULL) && (rc != TURTLE_RETURN_SUCCESS))
                _handler(rc, caller);
        return rc;
}

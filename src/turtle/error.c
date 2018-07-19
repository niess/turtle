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

#include "error.h"
/* C89 standard library */
#include <stdio.h>
#include <stdlib.h>

/* The library user supplied error handler. */
static turtle_handler_cb * _handler = NULL;

/* Setter for the error handler. */
void turtle_handler(turtle_handler_cb * handler) { _handler = handler; }

/* Utility function for handling errors. */
enum turtle_return turtle_error_raise(enum turtle_return rc,
    void (*caller)(void), const char * file, int line, const char * format, ...)
{
        if ((_handler == NULL) || (rc == TURTLE_RETURN_SUCCESS)) return rc;

/* Format the error message */
#define ERROR_MSG_LENGTH 1024
        char message[ERROR_MSG_LENGTH];
        const int n = snprintf(message, ERROR_MSG_LENGTH,
            "{ %s [#%d], %s:%d } ", turtle_strfunc(caller), rc, file, line);
        if (n < ERROR_MSG_LENGTH - 1) {
                va_list ap;
                va_start(ap, format);
                vsnprintf(message + n, ERROR_MSG_LENGTH - n, format, ap);
                va_end(ap);
        }
        _handler(rc, caller, message);

        return rc;
}

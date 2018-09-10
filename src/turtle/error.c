/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicUles over Long rangEs (TURTLE)
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
        turtle_finalise();
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
            "{ %s [#%d], %s:%d } ", turtle_strfunc(error_->function), rc, file,
            line);
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

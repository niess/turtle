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
 * Prototype(s) for error handling encapsulation.
 */
#ifndef TURTLE_ERROR_H
#define TURTLE_ERROR_H

#include "turtle.h"

#include <stdarg.h>

/* Helper macros for returning an encapsulated error. */
#define TURTLE_ERROR_MESSAGE(rc, caller, message)                              \
        turtle_error_raise(                                                    \
            rc, (turtle_caller_t *)caller, __FILE__, __LINE__, message)

#define TURTLE_ERROR_FORMAT(rc, caller, format, ...)                           \
        turtle_error_raise(rc, (turtle_caller_t *)caller, __FILE__, __LINE__,  \
            format, __VA_ARGS__)

#define TURTLE_ERROR_LOCK(caller)                                              \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_LOCK_ERROR, caller, "could not acquire the lock")

#define TURTLE_ERROR_UNLOCK(caller)                                            \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_UNLOCK_ERROR, caller, "could not release the lock")

#define TURTLE_ERROR_MISSING_DATA(caller, datum)                               \
        TURTLE_ERROR_FORMAT(TURTLE_RETURN_PATH_ERROR, caller,                  \
            "missing elevation data in `%s`", datum->root);

#define TURTLE_ERROR_MEMORY(caller)                                            \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_MEMORY_ERROR, caller, "could not allocate memory");

#define TURTLE_ERROR_EXTENSION(caller, extension)                              \
        TURTLE_ERROR_FORMAT(TURTLE_RETURN_BAD_EXTENSION, caller,               \
            "unsuported file format `%s`", extension);

#define TURTLE_ERROR_NO_EXTENSION(caller)                                      \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_BAD_EXTENSION, caller, "missing file extension");

#define TURTLE_ERROR_PATH(caller, path)                                        \
        TURTLE_ERROR_FORMAT(TURTLE_RETURN_PATH_ERROR, caller,                  \
            "could not find file `%s`", path);

#define TURTLE_ERROR_BOX(caller)                                               \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_DOMAIN_ERROR, caller, "invalid bounding box");

#define TURTLE_ERROR_OUTSIDE_MAP(caller)                                       \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_DOMAIN_ERROR, caller, "point is outside of map");

#define TURTLE_ERROR_UNEXPECTED(rc, caller)                                    \
        TURTLE_ERROR_MESSAGE(rc, caller, "an unexpected error occured");

/*
 * Utility function for encapsulating `returns` with a user
 * supplied error handler.
 */
enum turtle_return turtle_error_raise(enum turtle_return rc,
    void (*caller)(void), const char * file, int line, const char * format,
    ...);

#endif

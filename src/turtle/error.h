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

/* Storage for error data */
struct turtle_error_context {
        enum turtle_return code;
        turtle_function_t * function;
#define TURTLE_ERROR_MSG_LENGTH 1024
        char message[TURTLE_ERROR_MSG_LENGTH];
};

/* Helper macros for managing errors */
#define TURTLE_ERROR_INITIALISE(caller)                                        \
        struct turtle_error_context error_data = { .code =                     \
                                                       TURTLE_RETURN_SUCCESS,  \
                .function = (turtle_function_t *)caller };                     \
        struct turtle_error_context * error_ = &error_data;

#define TURTLE_ERROR_MESSAGE(rc, message)                                      \
        turtle_error_format(error_, rc, __FILE__, __LINE__, message),          \
            turtle_error_raise(error_)

#define TURTLE_ERROR_FORMAT(rc, format, ...)                                   \
        turtle_error_format(                                                   \
            error_, rc, __FILE__, __LINE__, format, __VA_ARGS__),              \
            turtle_error_raise(error_)

#define TURTLE_ERROR_REGISTER(rc, message)                                     \
        turtle_error_format(error_, rc, __FILE__, __LINE__, message)

#define TURTLE_ERROR_VREGISTER(rc, format, ...)                                \
        turtle_error_format(error_, rc, __FILE__, __LINE__, format, __VA_ARGS__)

#define TURTLE_ERROR_RAISE() turtle_error_raise(error_)

#define TURTLE_ERROR_LOCK()                                                    \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_LOCK_ERROR, "could not acquire the lock")

#define TURTLE_ERROR_UNLOCK()                                                  \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_UNLOCK_ERROR, "could not release the lock")

#define TURTLE_ERROR_MISSING_DATA(stack)                                       \
        TURTLE_ERROR_FORMAT(TURTLE_RETURN_PATH_ERROR,                          \
            "missing elevation data in `%s`", stack->root);

#define TURTLE_ERROR_MEMORY()                                                  \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_MEMORY_ERROR, "could not allocate memory");

#define TURTLE_ERROR_EXTENSION(extension)                                      \
        TURTLE_ERROR_FORMAT(TURTLE_RETURN_BAD_EXTENSION,                       \
            "unsuported file format `%s`", extension);

#define TURTLE_ERROR_NO_EXTENSION()                                            \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_BAD_EXTENSION, "missing file extension");

#define TURTLE_ERROR_PATH(path)                                                \
        TURTLE_ERROR_FORMAT(                                                   \
            TURTLE_RETURN_PATH_ERROR, "could not find file `%s`", path);

#define TURTLE_ERROR_BOX()                                                     \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_DOMAIN_ERROR, "invalid bounding box");

#define TURTLE_ERROR_OUTSIDE_MAP()                                             \
        TURTLE_ERROR_MESSAGE(                                                  \
            TURTLE_RETURN_DOMAIN_ERROR, "point is outside of map");

#define TURTLE_ERROR_UNEXPECTED(rc)                                            \
        TURTLE_ERROR_MESSAGE(rc, "an unexpected error occured");

/* Generic function for formating an error */
enum turtle_return turtle_error_format(struct turtle_error_context * error_,
    enum turtle_return rc, const char * file, int line, const char * format,
    ...);

/* Generic function for handling an error */
enum turtle_return turtle_error_raise(struct turtle_error_context * error_);

#endif

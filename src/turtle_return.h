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
#ifndef TURTLE_RETURN_H
#define TURTLE_RETURN_H

#include "turtle.h"

/* Helper macro for returning an encapsulated error code. */
#define TURTLE_RETURN(rc, caller)                                              \
        return turtle_return(rc, (turtle_caller_t *)caller)

/*
 * Utility function for encapsulating `returns` with a user supplied
 * error handler.
 */
enum turtle_return turtle_return(
    enum turtle_return rc, turtle_caller_t * caller);

#endif
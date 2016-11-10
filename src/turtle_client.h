/*
 *  Topographic Utilities for Rendering The eLEvation (TURTLE)
 *  Copyright (C) 2016  Valentin Niess
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Turtle client for multithreaded access to an elevation data handled by a
 * turtle_datum.
 */
#ifndef TURTLE_CLIENT_H
#define TURTLE_CLIENT_H

/* Container for a datum client. */
struct turtle_client {
        /* The currently used tile. */
        struct datum_tile * tile;

        /* The last requested indices */
        int index_la, index_lo;

        /* The master datum. */
        struct turtle_datum * datum;
};

#endif

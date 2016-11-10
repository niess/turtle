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
 * Turtle projection handle for dealing with geographic projections.
 */
#ifndef TURTLE_PROJECTION_H
#define TURTLE_PROJECTION_H

enum projection_type {
        PROJECTION_NONE = -1,
        PROJECTION_LAMBERT,
        PROJECTION_UTM,
        N_PROJECTIONS
};

/* Container for a geographic projection. */
struct turtle_projection {
        enum projection_type type;
        union {
                struct {
                        double longitude_0;
                        int hemisphere;
                } utm;
                int lambert_tag;
        } settings;
};

#endif

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

/* Tile loading functions for a turle_stack. */
#ifndef TURTLE_LOADER_H
#define TURTLE_LOADER_H

enum loader_format {
        LOADER_FORMAT_UNKNOWN = 0,
        LOADER_FORMAT_GEOTIFF,
        LOADER_FORMAT_HGT
};

enum loader_format loader_format(const char * path);
enum turtle_return loader_meta(const char * path, struct tile * tile);
enum turtle_return loader_load(const char * path, struct tile ** tile);

#endif

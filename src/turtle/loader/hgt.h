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
 * Interface to hgt files providing a reader for 16b data, e.g. SRTM tiles.
 */
#ifndef TURTLE_READER_HGT_H
#define TURTLE_READER_HGT_H

#include <stdint.h>
#include <stdio.h>

/* Data for reading an hgt file. */
struct hgt_reader {
        int size;
        double scale;
        double origin[2];
        FILE * fid;
};

/* Manage a geotiff 16b file reader. */
int hgt_open(const char * path, struct hgt_reader * reader);
void hgt_close(struct hgt_reader * reader);
int hgt_readinto(struct hgt_reader * reader, int16_t * buffer);

#endif

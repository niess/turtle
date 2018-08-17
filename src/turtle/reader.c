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

/* Generic map reader for the TURTLE library */

/* C89 standard library */
#include <stdlib.h>
#include <string.h>
/* TURTLE library */
#include "turtle/reader.h"

/* Generic reader constructor type */
typedef enum turtle_return reader_creator_t(
    struct turtle_reader ** reader, struct turtle_error_context * error_);

struct reader_info {
        const char * extension;
        reader_creator_t * create;
};

/* List of available readers */
#ifndef TURTLE_NO_PNG
extern enum turtle_return turtle_reader_png16_create(
    struct turtle_reader ** reader, struct turtle_error_context * error_);
#endif
#ifndef TURTLE_NO_HGT
extern enum turtle_return turtle_reader_hgt_create(
    struct turtle_reader ** reader, struct turtle_error_context * error_);
#endif
#ifndef TURTLE_NO_TIFF
extern enum turtle_return turtle_reader_geotiff16_create(
    struct turtle_reader ** reader, struct turtle_error_context * error_);
#endif

static struct reader_info info[] = {
#ifndef TURTLE_NO_PNG
        { "png", &turtle_reader_png16_create },
#endif
#ifndef TURTLE_NO_HGT
        { "hgt", &turtle_reader_hgt_create },
#endif
#ifndef TURTLE_NO_TIFF
        { "tif", &turtle_reader_geotiff16_create },
#endif
};

/* Generic reader allocator, given a file name */
enum turtle_return turtle_reader_create_(struct turtle_reader ** reader,
    const char * path, struct turtle_error_context * error_)
{
        /* Get the file extension */
        *reader = NULL;
        const char * extension = strrchr(path, '.');
        if (extension != NULL)
                extension++;
        else
                goto error;

        /* Look for a reader */
        const int n = sizeof(info) / sizeof(*info);
        int i;
        for (i = 0; i < n; i++) {
                if (strcmp(info[i].extension, extension) == 0) {
                        enum turtle_return rc = info[i].create(reader, error_);
                        if (rc == TURTLE_RETURN_SUCCESS)
                                strcpy((*reader)->meta.encoding, extension);
                        return rc;
                }
        }
error:
        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_EXTENSION,
            "no valid reader for file `%s` ", path);
}

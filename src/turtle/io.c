/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicules over Long rangEs (TURTLE)
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

/* Generic read/write for the TURTLE library */

/* C89 standard library */
#include <stdlib.h>
#include <string.h>
/* TURTLE library */
#include "turtle/io.h"

/* Generic reader constructor type */
typedef enum turtle_return io_creator_t(
    struct turtle_io ** io, struct turtle_error_context * error_);

struct io_info {
        const char * extension;
        io_creator_t * create;
};

/* List of available formats */
#ifndef TURTLE_NO_TIFF
extern enum turtle_return turtle_io_geotiff16_create_(
    struct turtle_io ** io, struct turtle_error_context * error_);
#endif
#ifndef TURTLE_NO_GRD
extern enum turtle_return turtle_io_grd_create_(
    struct turtle_io ** io, struct turtle_error_context * error_);
#endif
#ifndef TURTLE_NO_HGT
extern enum turtle_return turtle_io_hgt_create_(
    struct turtle_io ** io, struct turtle_error_context * error_);
#endif
#ifndef TURTLE_NO_PNG
extern enum turtle_return turtle_io_png16_create_(
    struct turtle_io ** io, struct turtle_error_context * error_);
#endif

static struct io_info info[] = {
#ifndef TURTLE_NO_TIFF
        { "tif", &turtle_io_geotiff16_create_ },
#endif
#ifndef TURTLE_NO_GRD
        { "grd", &turtle_io_grd_create_ },
#endif
#ifndef TURTLE_NO_HGT
        { "hgt", &turtle_io_hgt_create_ },
#endif
#ifndef TURTLE_NO_PNG
        { "png", &turtle_io_png16_create_ },
#endif
};

/* Generic io allocator, given a file name */
enum turtle_return turtle_io_create_(struct turtle_io ** io, const char * path,
    struct turtle_error_context * error_)
{
        /* Get the file extension */
        *io = NULL;
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
                        enum turtle_return rc = info[i].create(io, error_);
                        if (rc == TURTLE_RETURN_SUCCESS)
                                strcpy((*io)->meta.encoding, extension);
                        return rc;
                }
        }
error:
        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_EXTENSION,
            "no valid format for file `%s'", path);
}

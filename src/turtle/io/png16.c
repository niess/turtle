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

/*
 * I/O's to png files providing a read/write for 16b/grayscale data
 */

/* C89 standard library */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Endianess utilities */
#include <arpa/inet.h>
#ifndef TURTLE_NO_LD
/* Dynamic libraries */
#include <dlfcn.h>
#endif
/* PNG library */
#ifndef TURTLE_NO_LD
#include "deps/png.h"
#else
#include <png.h>
#endif
/* JSON parser */
#include "deps/jsmn.h"
/* TURTLE library */
#include "turtle/io.h"

/* Data for accessing a png file */
struct png16_io {
        /* Base io object */
        struct turtle_io base;

        /* Internal data for the io */
        int read;
        FILE * fid;
        png_structp png_ptr;
        png_infop info_ptr;
};

/* Libpng API */
static struct {
        void * lib;
        png_charp version;

        int (*sig_cmp) (png_bytep, png_size_t, png_size_t);
        jmp_buf * (*set_longjmp_fn) (png_structp, png_longjmp_ptr, size_t);
        png_byte (*get_bit_depth) (png_structp, png_infop);
        png_byte (*get_color_type) (png_structp, png_infop);
        png_charp (*get_libpng_ver) (png_structp);
        png_infop (*create_info_struct) (png_structp);
        png_structp (*create_read_struct) (png_const_charp, png_voidp,
            png_error_ptr, png_error_ptr);
        png_structp (*create_write_struct) (png_const_charp, png_voidp,
            png_error_ptr, png_error_ptr);
        png_uint_32 (*get_image_height) (png_structp, png_infop);
        png_uint_32 (*get_image_width) (png_structp, png_infop);
        png_uint_32 (*get_rowbytes) (png_structp, png_infop);
        png_uint_32 (*get_text) (png_structp, png_infop, png_textp *, int *);
        void (*destroy_read_struct) (png_structpp, png_infopp, png_infopp);
        void (*destroy_write_struct) (png_structpp, png_infopp);
        void (*init_io) (png_structp, png_FILE_p);
        void (*read_image) (png_structp, png_bytepp);
        void (*read_info) (png_structp, png_infop);
        void (*read_update_info) (png_structp, png_infop);
        void (*set_IHDR) (png_structp, png_infop, png_uint_32, png_uint_32,
            int, int, int, int, int);
        void (*set_sig_bytes) (png_structp, int);
        void (*set_text) (png_structp, png_infop, png_textp, int);
        void (*write_end) (png_structp, png_infop);
        void (*write_image) (png_structp, png_bytepp);
        void (*write_info) (png_structp, png_infop);
} api = { NULL };

static enum turtle_return api_initialise(struct turtle_error_context * error_)
{
#ifndef TURTLE_NO_LD
#ifdef __APPLE__
#define SOEXT "dylib"
#else
#define SOEXT "so"
#endif
        if (api.lib != NULL)
                return TURTLE_RETURN_SUCCESS;

        api.lib = dlopen("libpng." SOEXT, RTLD_LAZY);
        if (api.lib == NULL) {
                return TURTLE_ERROR_REGISTER(
                    TURTLE_RETURN_PATH_ERROR, dlerror());
        }

#define LINK(NAME)                                                             \
        if ((api. NAME = dlsym(api.lib, "png_" #NAME)) == NULL)                \
                goto error
#else
        static int initialised = 0;
        if (initialised) return TURTLE_RETURN_SUCCESS;
        initialised = 1;

#define LINK(NAME) api. NAME = (void *)&png_ ##NAME
#endif

        LINK(sig_cmp);
        LINK(create_read_struct);
        LINK(create_info_struct);
        LINK(init_io);
        LINK(read_info);
        LINK(set_sig_bytes);
        LINK(get_color_type);
        LINK(get_libpng_ver);
        LINK(get_bit_depth);
        LINK(get_image_width);
        LINK(get_image_height);
        LINK(read_update_info);
        LINK(get_text);
        LINK(destroy_read_struct);
        LINK(destroy_write_struct);
        LINK(get_rowbytes);
        LINK(read_image);
        LINK(create_write_struct);
        LINK(set_IHDR);
        LINK(set_text);
        LINK(write_info);
        LINK(write_image);
        LINK(write_end);
        LINK(set_longjmp_fn);

        api.version = api.get_libpng_ver(NULL);
        const char min_version = '4';
        if ((strlen(api.version) < 3) || (api.version[2] < min_version)) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "bad libpng version (expected >= 1.%c, got %s)",
                    min_version, api.version);
        }

        return TURTLE_RETURN_SUCCESS;

#ifndef TURTLE_NO_LD
error:
        dlclose(api.lib);
        api.lib = NULL;
        return TURTLE_ERROR_REGISTER(TURTLE_RETURN_BAD_FORMAT, dlerror());

#undef LINK
#undef SOEXT
#endif
}

static jmp_buf * get_jmpbuf(png_structp png_ptr)
{
        return api.set_longjmp_fn(png_ptr, longjmp, sizeof (jmp_buf));
}

static int json_strcmp(
    const char * json, jsmntok_t * token, const char * string)
{
        return strncmp(json + token->start, string, token->end - token->start);
}

static int json_sscanf(const char * json, jsmntok_t * token, double * value)
{
        return sscanf(json + token->start, "%la", value);
}

static enum turtle_return png16_open(struct turtle_io * io, const char * path,
    const char * mode, struct turtle_error_context * error_)
{
        struct png16_io * png16 = (struct png16_io *)io;
        if (png16->fid != NULL) io->close(io);

        /* Open the file and check the format */
        png16->fid = fopen(path, mode);
        if (png16->fid == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }

        if (mode[0] != 'r') {
                png16->read = 0;
                return TURTLE_RETURN_SUCCESS;
        } else
                png16->read = 1;

        /* Initialise the meta data */
        io->meta.nx = io->meta.ny = 0;
        io->meta.x0 = io->meta.y0 = io->meta.z0 = 0.;
        io->meta.dx = io->meta.dy = io->meta.dz = 0.;
        io->meta.projection.type = PROJECTION_NONE;

        char header[8];
        if ((fread(header, 1, 8, png16->fid) != 8) ||
            (api.sig_cmp((png_bytep)header, 0, 8) != 0)) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "invalid header for png `%s'", path);
        }

        /* initialize libpng containers */
        png16->png_ptr =
            api.create_read_struct(api.version, NULL, NULL, NULL);
        if (png16->png_ptr == NULL) {
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "could not create png proxy for file `%s'", path);
        }
        png16->info_ptr = api.create_info_struct(png16->png_ptr);
        if (png16->info_ptr == NULL) {
                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "could not create png info for file `%s'", path);
                goto error;
        }
        if (setjmp(*get_jmpbuf(png16->png_ptr))) {
                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "a libpng error occured when loading file `%s'", path);
                goto error;
        }
        api.init_io(png16->png_ptr, png16->fid);
        api.set_sig_bytes(png16->png_ptr, 8);

        /* Read the header */
        api.read_info(png16->png_ptr, png16->info_ptr);
        if (api.get_color_type(png16->png_ptr, png16->info_ptr) !=
            PNG_COLOR_TYPE_GRAY) {
                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "invalid color scheme for png file `%s'", path);
                goto error;
        }
        png_byte bit_depth = api.get_bit_depth(png16->png_ptr, png16->info_ptr);
        if (bit_depth != 16) {
                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "invalid bit depth (%d != 16) for file `%s'", bit_depth,
                    path);
                goto error;
        }
        io->meta.nx = api.get_image_width(png16->png_ptr, png16->info_ptr);
        io->meta.ny = api.get_image_height(png16->png_ptr, png16->info_ptr);
        api.read_update_info(png16->png_ptr, png16->info_ptr);

        /* Parse the JSON meta data */
        png_textp text_ptr;
        int num_text;
        unsigned n_chunks =
            api.get_text(png16->png_ptr, png16->info_ptr, &text_ptr, &num_text);
        if (n_chunks > 0) {
                int i = 0;
                png_text * p = text_ptr;
                for (; i < num_text; i++) {
                        char * text = p->text;
                        unsigned int length = p->text_length;
                        p++;

                        /* Map all the tokens with JSMN */
                        jsmn_parser parser;
#define N_TOKENS 17
                        jsmntok_t tokens[N_TOKENS];
                        jsmn_init(&parser);
                        const int r =
                            jsmn_parse(&parser, text, length, tokens, N_TOKENS);
                        if (r < N_TOKENS) continue;

                        /* Check for a "topography" object. */
                        if ((tokens[0].type != JSMN_OBJECT) ||
                            (tokens[1].type != JSMN_STRING))
                                continue;
                        if (json_strcmp(text, tokens + 1, "topography") != 0)
                                continue;
#define N_FIELDS 7
                        if ((tokens[2].type != JSMN_OBJECT) ||
                            (tokens[2].size != N_FIELDS)) {
                                TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                                    "invalid meta data for png file `%s'",
                                    path);
                                goto error;
                        }

                        /* Parse the topography fields */
                        double x1, y1, z1;
                        int j, done[N_FIELDS];
                        jsmntok_t *key, *value;
                        memset(done, 0x0, sizeof(done));
                        for (j = 0, key = tokens + 3, value = tokens + 4;
                             j < N_FIELDS; j++, key += 2, value += 2) {
                                if (!done[0] && (json_strcmp(text, key,
                                                     "projection") == 0)) {
                                        if (value->type != JSMN_STRING) {
                                                TURTLE_ERROR_VREGISTER(
                                                    TURTLE_RETURN_BAD_FORMAT,
                                                    "invalid projection for "
                                                    "png file `%s'",
                                                    path);
                                                goto error;
                                        }
                                        if (value->end > value->start) {
                                                text[value->end] = '\0';
                                                if (turtle_projection_configure_(
                                                        &io->meta.projection,
                                                        text + value->start,
                                                        error_) !=
                                                    TURTLE_RETURN_SUCCESS)
                                                        goto error;
                                        }
                                        done[0] = 1;
                                } else {
                                        if (value->type != JSMN_PRIMITIVE) {
                                                TURTLE_ERROR_VREGISTER(
                                                    TURTLE_RETURN_BAD_FORMAT,
                                                    "invalid type for `%s' in "
                                                    "png file `%s'",
                                                    key, path);
                                                goto error;
                                        }

                                        if (!done[1] && (json_strcmp(text, key,
                                                             "x0") == 0)) {
                                                json_sscanf(
                                                    text, value, &io->meta.x0);
                                                done[1] = 1;
                                        } else if (!done[2] &&
                                            (json_strcmp(text, key, "y0") ==
                                                0)) {
                                                json_sscanf(
                                                    text, value, &io->meta.y0);
                                                done[2] = 1;
                                        } else if (!done[3] &&
                                            (json_strcmp(text, key, "z0") ==
                                                0)) {
                                                json_sscanf(
                                                    text, value, &io->meta.z0);
                                                done[3] = 1;
                                        } else if (!done[4] &&
                                            (json_strcmp(text, key, "x1") ==
                                                0)) {
                                                json_sscanf(text, value, &x1);
                                                done[4] = 1;
                                        } else if (!done[5] &&
                                            (json_strcmp(text, key, "y1") ==
                                                0)) {
                                                json_sscanf(text, value, &y1);
                                                done[5] = 1;
                                        } else if (!done[6] &&
                                            (json_strcmp(text, key, "z1") ==
                                                0)) {
                                                json_sscanf(text, value, &z1);
                                                done[6] = 1;
                                        } else {
                                                TURTLE_ERROR_VREGISTER(
                                                    TURTLE_RETURN_BAD_FORMAT,
                                                    "invalid key `%s' for png "
                                                    "file `%s'",
                                                    key, path);
                                                goto error;
                                        }
                                }
                        }
                        io->meta.dx = (x1 - io->meta.x0) / (io->meta.nx - 1);
                        io->meta.dy = (y1 - io->meta.y0) / (io->meta.ny - 1);
                        io->meta.dz = (z1 - io->meta.z0) / 65535;
#undef N_FIELDS
#undef N_TOKENS
                }
        }

        return TURTLE_RETURN_SUCCESS;
error:
        /* Close and return if an error occurred */
        io->close(io);
        return error_->code;
}

static void png16_close(struct turtle_io * io)
{
        struct png16_io * png16 = (struct png16_io *)io;
        if (png16->fid != NULL) {
                fclose(png16->fid);
                png16->fid = NULL;
                png_structpp pp_p =
                    (png16->png_ptr != NULL) ? &png16->png_ptr : NULL;
                png_infopp pp_i =
                    (png16->info_ptr != NULL) ? &png16->info_ptr : NULL;
                if (png16->read)
                        api.destroy_read_struct(pp_p, pp_i, NULL);
                else
                        api.destroy_write_struct(pp_p, pp_i);
                png16->png_ptr = NULL;
                png16->info_ptr = NULL;
        }
}

static double get_z(const struct turtle_map * map, int ix, int iy)
{
        iy = map->meta.ny - 1 - iy;
        const uint16_t iz = (uint16_t)ntohs(map->data[iy * map->meta.nx + ix]);
        return map->meta.z0 + iz * map->meta.dz;
}

static void set_z(struct turtle_map * map, int ix, int iy, double z)
{
        const double d = round((z - map->meta.z0) / map->meta.dz);
        iy = map->meta.ny - 1 - iy;
        map->data[iy * map->meta.nx + ix] = (uint16_t)htons(d);
}

static enum turtle_return png16_read(struct turtle_io * io,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct png16_io * png16 = (struct png16_io *)io;

        /* Load the data */
        png_bytep * row_pointers = calloc(map->meta.ny, sizeof(png_bytep));
        if (row_pointers == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for png rows");
        }
        int i = 0;
        for (; i < map->meta.ny; i++) {
                row_pointers[i] =
                    malloc(api.get_rowbytes(png16->png_ptr, png16->info_ptr));
                if (row_pointers[i] == NULL) {
                        TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                            "could not allocate memory for png row");
                        goto exit;
                }
        }
        api.read_image(png16->png_ptr, row_pointers);

        /* Copy the data to the map */
        uint16_t * z16 = map->data;
        for (i = 0; i < map->meta.ny; i++) {
                memcpy(z16, row_pointers[i], map->meta.nx * sizeof(*z16));
                z16 += map->meta.nx;
        }
exit:
        /* Clean and return */
        for (i = 0; i < map->meta.ny; i++) free(row_pointers[i]);
        free(row_pointers);
        return error_->code;
}

/* Dump a map in png format */
static enum turtle_return png16_write(struct turtle_io * io,
    const struct turtle_map * map, struct turtle_error_context * error_)
{
        png_bytep * row_pointers = NULL;
        int header_size = 2048;
        char * header = NULL;

        struct png16_io * png16 = (struct png16_io *)io;
        png16->png_ptr =
            api.create_write_struct(api.version, NULL, NULL, NULL);
        if (png16->png_ptr == NULL) {
                TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for png proxy");
                goto exit;
        }
        png16->info_ptr = api.create_info_struct(png16->png_ptr);
        if (png16->info_ptr == NULL) {
                TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for png info");
                goto exit;
        }
        if (setjmp(*get_jmpbuf(png16->png_ptr))) goto exit;
        api.init_io(png16->png_ptr, png16->fid);

        /* Write the header */
        const int nx = map->meta.nx, ny = map->meta.ny;
        api.set_IHDR(png16->png_ptr, png16->info_ptr, nx, ny, 16,
            PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);
        const char * tmp;
        const struct turtle_projection * projection = &map->meta.projection;
        const char * projection_tag = turtle_projection_name(projection);
        if (projection_tag == NULL)
                tmp = "";
        else
                tmp = projection_tag;
        for (;;) {
                char * new = realloc(header, header_size);
                if (new == NULL) goto exit;
                header = new;
                const double x1 =
                    map->meta.x0 + map->meta.dx * (map->meta.nx - 1);
                const double y1 =
                    map->meta.y0 + map->meta.dy * (map->meta.ny - 1);
                const double z1 = map->meta.z0 + map->meta.dz * 65535;
                if (snprintf(header, header_size,
                        "{\"topography\" : {\"x0\" : %a, \"y0\" : %a, "
                        "\"z0\" : %a, \"x1\" : %a, \"y1\" : %a, "
                        "\"z1\" : %a, \"projection\" : \"%s\"}}",
                        map->meta.x0, map->meta.y0, map->meta.z0, x1, y1, z1,
                        tmp) < header_size)
                        break;
                header_size += 2048;
        }
        png_text text[] = { { PNG_TEXT_COMPRESSION_NONE, "Comment", header,
            strlen(header) } };
        api.set_text(png16->png_ptr, png16->info_ptr, text,
            sizeof(text) / sizeof(text[0]));
        api.write_info(png16->png_ptr, png16->info_ptr);
        free(header);
        header = NULL;

        /* Write the data */
        row_pointers = (png_bytep *)calloc(ny, sizeof(png_bytep));
        if (row_pointers == NULL) goto exit;
        int i;
        for (i = 0; i < ny; i++) {
                row_pointers[i] = (png_byte *)malloc(nx * sizeof(uint16_t));
                if (row_pointers[i] == NULL) goto exit;
                uint16_t * ptr = (uint16_t *)row_pointers[i];
                int j;
                for (j = 0; j < nx; j++) {
                        const double d =
                            round((map->meta.get_z(map, j, ny - 1 - i) -
                                      map->meta.z0) /
                                map->meta.dz);
                        *(ptr++) = (uint16_t)htons(d);
                }
        }
        api.write_image(png16->png_ptr, row_pointers);
        api.write_end(png16->png_ptr, NULL);

exit:
        /* Clean and return */
        free(header);
        if (row_pointers != NULL) {
                for (i = 0; i < ny; i++) free(row_pointers[i]);
                free(row_pointers);
        }
        return error_->code;
}

enum turtle_return turtle_io_png16_create_(
    struct turtle_io ** io_p, struct turtle_error_context * error_)
{
        enum turtle_return rc;
        if ((rc = api_initialise(error_)) != TURTLE_RETURN_SUCCESS)
                return rc;

        /* Allocate the png16 io manager */
        struct png16_io * png16 = malloc(sizeof(*png16));
        if (png16 == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for png16 format");
        }
        *io_p = &png16->base;

        /* Initialise the io object */
        memset(png16, 0x0, sizeof(*png16));
        png16->fid = NULL;
        png16->png_ptr = NULL;
        png16->info_ptr = NULL;
        png16->base.meta.projection.type = PROJECTION_NONE;

        png16->base.open = &png16_open;
        png16->base.close = &png16_close;
        png16->base.read = &png16_read;
        png16->base.write = &png16_write;

        png16->base.meta.get_z = &get_z;
        png16->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}

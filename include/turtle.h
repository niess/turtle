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

#ifndef TURTLE_H
#define TURTLE_H
#ifdef __cplusplus
extern "C" {
#endif

/* Prefix for TURTLE library functions */
#ifndef TURTLE_API
#define TURTLE_API
#endif

/**
 * Return Codes used by the TURTLE library
 */
enum turtle_return {
        /** The operation succeeded */
        TURTLE_RETURN_SUCCESS = 0,
        /** A wrong pointer address was provided, e.g. NULL */
        TURTLE_RETURN_BAD_ADDRESS,
        /** A provided file extension is not supported or recognised */
        TURTLE_RETURN_BAD_EXTENSION,
        /** A provided file or string has a wrong format */
        TURTLE_RETURN_BAD_FORMAT,
        /** A provided `turtle_projection` is not supported */
        TURTLE_RETURN_BAD_PROJECTION,
        /** Some JSON metadata couldn't be understood */
        TURTLE_RETURN_BAD_JSON,
        /** Some input parameters are out of their validity range */
        TURTLE_RETURN_DOMAIN_ERROR,
        /** An TURTLE low level library error occurred */
        TURTLE_RETURN_LIBRARY_ERROR,
        /** A lock couldn't be acquired */
        TURTLE_RETURN_LOCK_ERROR,
        /** Some memory couldn't be allocated */
        TURTLE_RETURN_MEMORY_ERROR,
        /** A provided path wasn't found */
        TURTLE_RETURN_PATH_ERROR,
        /** A lock couldn't be released */
        TURTLE_RETURN_UNLOCK_ERROR,
        /** The number of TURTLE error codes */
        N_TURTLE_RETURNS
};

/**
 * Opaque structure for geographic projections
 */
struct turtle_projection;

/**
 * Opaque structure for elevation maps
 */
struct turtle_map;

/**
 * Opaque structure for a stack of global topography data
 */
struct turtle_stack;

/**
 * Opaque structure for a stack client, providing thread safe access to
 * elevation data
 */
struct turtle_client;

/**
 * Opaque structure for stepping through geographic data
 */
struct turtle_stepper;

/**
 * Meta data for elevation maps
 */
struct turtle_map_info {
        /** Number of grid nodes along X */
        int nx;
        /** Number of grid nodes along Y */
        int ny;
        /** X coordinate range (min, max) */
        double x[2];
        /** Y coordinate range (min, max) */
        double y[2];
        /** Z coordinate range (min, max) */
        double z[2];
        /** Data encoding format */
        const char * encoding;
};

/**
 * Generic function pointer
 *
 * This is a generic function pointer used to identify TURTLE's library
 * functions, e.g. for error handling.
 */
typedef void turtle_function_t(void);

/**
 * Callback for handling TURTLE library errors
 *
 * @param code      The TURTLE return code
 * @param caller    The caller function where the error occurred
 * @param message   A formatted message describing the error
 *
 * The user might provide its own error handler. By default errors are printed
 * to `stderr` and a hard exit occurs.
 *
 * **Note** : providing a `NULL` error handler disables error handling.
 * Nevertheless, the TURTLE library functions will still return and error code.
 *
 * __Warnings__
 *
 * This callback *must* be thread safe if a `turtle_client` is used.
 */
typedef void turtle_error_handler_t(enum turtle_return code,
    turtle_function_t * function, const char * message);

/**
 * Callbacks for managing concurrent accesses to the stack
 *
 * @return `0` on success, any other value otherwise.
 *
 * For multi-threaded applications with a `turtle_stack` and `turtle_client`s
 * the user must supply a `lock` and `unlock` callback providing exclusive
 * access to critical sections, e.g. using a semaphore.
 *
 * __Warnings__
 *
 * The callback *must* return `0` if the (un)lock was successful.
 */
typedef int turtle_stack_locker_t(void);

/**
 * Return a string describing a TURTLE library function
 *
 * This function is meant for verbosing when handling errors. It is thread
 * safe.
 */
TURTLE_API const char * turtle_error_function(turtle_function_t * function);

/**
 * Get the library error handler
 *
 * @return The current error handler
 *
 * This function allows to set or alter the error handler. Only one error
 * handler can be set at a time for all threads. It is not thread safe
 * to modify it. By default errors are printed to `stderr` and a hard exit
 * occurs.
 *
 * **Note** : providing a `NULL` error handler disables error handling.
 * Nevertheless, the TURTLE library functions will still return and error code.
 */
TURTLE_API turtle_error_handler_t * turtle_error_handler_get(void);

/**
 * Set the library error handler
 *
 * @param handler    The user supplied error handler
 *
 * This function allows to set or alter the error handler. Only one error
 * handler can be set at a time for all threads. It is not thread safe
 * to modify it. By default errors are printed to `stderr` and a hard exit
 * occurs.
 *
 * Note that providing a `NULL` error handler disables error handling.
 * Nevertheless, the TURTLE library functions will still return and error code.
 *
 * Note also that this function can be called before the library
 * initialisation.
 *
 * __Warnings__
 *
 * This function is not thread safe.
 */
TURTLE_API void turtle_error_handler_set(turtle_error_handler_t * handler);

/**
 * Create a new geographic projection
 *
 * @param projection    The projection object
 * @param name          The name tag of the projection
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * The currently supported projections are **Lambert** and **UTM**. The *name*
 * tag encodes the projection parameters. It must be one of the followings:
 *
 *    * Lambert I
 *
 *    * Lambert II
 *
 *    * Lambert IIe    (for Lambert II extended)
 *
 *    * Lambert III
 *
 *    * Lambert IV
 *
 *    * Lambert 93     (conforming to RGF93)
 *
 *    * UTM {zone}{hemisphere}
 *
 *    * UTM {longitude}{hemisphere}
 *
 * where {zone} is an integer in [1, 60] encoding the UTM world zone and
 * hemisphere must be `N` for north or `S` for south, e.g. `UTM 31N` for the
 * centre of France. Alternatively the central longitude of the UTM projection
 * can be provided directly as an explicit floating number, e.g. `UTM 3.0N` for
 * the UTM zone 31N as previously.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The name tag isn't valid
 *
 *    TURTLE_RETURN_MEMORY_ERROR      The projection could not be allocated
 */
TURTLE_API enum turtle_return turtle_projection_create(
    struct turtle_projection ** projection, const char * name);

/**
 * Destroy a geographic projection
 *
 * @param projection    The projection object
 *
 * Fully destroy a geographic projection previously allocated with
 * `turtle_projection_create`. On return `projection` is set to `NULL`.
 *
 * __Warnings__
 *
 * This must **not** be called on a projection object returned by a
 * `turtle_map`. Instead one must call `turtle_map_destroy` to get rid of
 * both the map and its projection.
 */
TURTLE_API void turtle_projection_destroy(
    struct turtle_projection ** projection);

/**
 * (Re-)configure a geographic projection
 *
 * @param projection    The projection object
 * @param name          The name tag of the projection
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * See `turtle_projection_create` for the supported projections and valid
 * values for the `name` parameter.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The projection isn't supported
 */
TURTLE_API enum turtle_return turtle_projection_configure(
    struct turtle_projection * projection, const char * name);

/**
 * Get the name tag of a geographic projection
 *
 * @param projection    The projection object
 * @return The projection name tag or `ǸULL`
 *
 * This function returns a `name` tag encoding the projection details. The
 * resulting name conforms to the inputs to `turtle_projection_configure`
 * or `turtle_projection_create`, e.g. `UTM 31N`. See the later for a
 * detailed description.
 */
TURTLE_API const char * turtle_projection_name(
    const struct turtle_projection * projection);

/**
 * Apply a geographic projection to geodetic coordinates
 *
 * @param projection    The projection object
 * @param latitude      The input geodetic latitude
 * @param latitude      The input geodetic longitude
 * @param x             The output X-coordinate
 * @param y             The output Y-coordinate
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Apply the geographic projection to a set of geodetic coordinates. See
 * `turtle_projection_unproject` for the reverse transform.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS       The projection is `NULL`
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The projection isn't supported
 */
TURTLE_API enum turtle_return turtle_projection_project(
    const struct turtle_projection * projection, double latitude,
    double longitude, double * x, double * y);

/**
 * Unfold a geographic projection to recover the geodetic coordinates
 *
 * @param projection    The projection object
 * @param x             The input X-coordinate
 * @param y             The input Y-coordinate
 * @param latitude      The output geodetic latitude
 * @param latitude      The output geodetic longitude
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Unfold a geographic projection to recover the initial geodetic coordinates.
 * See `turtle_projection_project` for the direct transform.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS       The projection is `NULL`
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The provided projection isn't supported
 */
TURTLE_API enum turtle_return turtle_projection_unproject(
    const struct turtle_projection * projection, double x, double y,
    double * latitude, double * longitude);

/**
 * Create a new map
 *
 * @param map           The map object
 * @param info          The map meta data
 * @param projection    A geographic projection, or `NULL`
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Allocate memory for a new map and initialise it. Use `turtle_map_destroy`
 * in order to recover the allocated memory. The map is initialised as flat
 * with `info:nx x info:ny` static nodes of elevation `zmin`. The nodes are
 * distributed over a regular grid defined by `info`. The elevation
 * values are stored over 16 bits between `info:z[0]` and `info:z[1]`. If
 * `projection` is not `NULL` the map is initialised with a geographic
 * projection handle. See `turtle_projection_create` for a list of valid
 * projections and their names.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PROJECTION    An invalid projection was provided
 *
 *    TURTLE_RETURN_DOMAIN_ERROR      An input parameter is out of its
 * validity range
 *
 *    TURTLE_RETURN_MEMORY_ERROR      The map couldn't be allocated
 */
TURTLE_API enum turtle_return turtle_map_create(struct turtle_map ** map,
    const struct turtle_map_info * info, const char * projection);

/**
 * Destroy a projection map
 *
 * @param map    The map object
 *
 * Fully destroy a projection map and recover the memory previously allocated
 * by `turtle_projection_create`. On return `map` is set to `NULL`.
 */
TURTLE_API void turtle_map_destroy(struct turtle_map ** map);

/**
 * Load a map
 *
 * @param map     The map object
 * @param path    The path to the map file
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Load a map from a file. The file format is guessed from the filename
 * extension. Currently only `.png` and `.grd` (e.g. EGM96) formats are
 * supported.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_EXTENSION    The file format is not supported
 *
 *    TURTLE_RETURN_BAD_PATH         The file wasn't found
 *
 *    TURTLE_RETURN_MEMORY_ERROR     The map couldn't be allocated
 *
 *    TURTLE_RETURN_JSON_ERROR       The JSON metadata are invalid (.png file)
 *
 */
TURTLE_API enum turtle_return turtle_map_load(
    struct turtle_map ** map, const char * path);

/**
 * Dump a map to a file
 *
 * @param map     The map object
 * @param path    The path for the output file
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Dump a projection map to a file. The file format is guessed from the output
 * filename extension. Currently only a custom `.png` format is supported.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_EXTENSION    The file format is not supported
 *
 *    TURTLE_RETURN_BAD_FORMAT       The map cannot be dumped in the given
 * format, e.g. 16 bit
 *
 *    TURTLE_RETURN_BAD_PATH         The file couldn't be opened
 *
 *    TURTLE_RETURN_MEMORY_ERROR     Some temporary memory couldn't be
 * allocated
 */
TURTLE_API enum turtle_return turtle_map_dump(
    const struct turtle_map * map, const char * path);

/**
 * Fill the elevation value of a map node
 *
 * @param map          The map object
 * @param ix           The node X-coordinate
 * @param iy           The node Y-coordinate
 * @param elevation    The elevation value to set
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Fill the elevation value of the map node of coordinates `(ix, iy)`. The
 * elevation value must be in the range `[zmin, zmax]` of the `map`. **Note**
 * that due to digitization the actual node elevation can differ from the input
 * value by `(zmax-zmin)/65535`, e.g 1.5 cm for a 1 km altitude span.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR    Some input parameter isn't valid
 */
TURTLE_API enum turtle_return turtle_map_fill(
    struct turtle_map * map, int ix, int iy, double elevation);

/**
 * Get the properties of a map node
 *
 * @param map          The map object
 * @param ix           The node X-coordinate
 * @param iy           The node Y-coordinate
 * @param x            The node geographic X-coordinate
 * @param y            The node geographic Y-coordinate
 * @param elevation    The node elevation value
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise
 * `TURTLE_RETURN_DOMAIN_ERROR` if any input parameter isn't valid
 *
 * Get the properties of a map node, i.e. its geographic coordinates and
 * elevation value.
 */
TURTLE_API enum turtle_return turtle_map_node(const struct turtle_map * map,
    int ix, int iy, double * x, double * y, double * elevation);

/**
 * Get the map elevation at a geographic coordinate
 *
 * @param map          The map object
 * @param x            The geographic X-coordinate
 * @param y            The geographic Y-coordinate
 * @param elevation    The elevation value
 * @param inside       Flag for bounds check or `NULL`
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Compute an estimate of the map elevation at the given geographic
 * coordinates. The elevation is linearly interpolated using the 4 nodes that
 * surround the given coordinate. Providing a non `NULL` value for *inside*
 * allows to check if the provided coordinates are inside the map or not.
 * **Note** that no bound error is raised in the latter case, when *inside* is
 * not `NULL`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR    The coordinates are not valid
 */
TURTLE_API enum turtle_return turtle_map_elevation(
    const struct turtle_map * map, double x, double y, double * elevation,
    int * inside);

/**
 * Get the map's projection
 *
 * @param map    The map object
 * @return The map projection or `NULL` if none or if the map is `NULL`
 *
 * **Note** The provided projection object allows to set and modify the map's
 * projection, e.g. using `turtle_projection_configure`.
 */
TURTLE_API const struct turtle_projection * turtle_map_projection(
    const struct turtle_map * map);

/**
 * Get basic information on a projection map
 *
 * @param map          The map object
 * @param info         A pointer to a `turtle_map_info` structure
 * @param projection   A pointer for returning the projection, or `NULL`
 *
 * Get some basic information on a map. The meta data are filled
 * to the provided *info* structure. If *projection* is non `NULL` it
 * points to the map projection at return.
 */
TURTLE_API void turtle_map_meta(const struct turtle_map * map,
    struct turtle_map_info * info, const char ** projection);

/**
 * Transform geodetic coordinates to Cartesian ECEF ones
 *
 * @param latitude     The geodetic latitude
 * @param longitude    The geodetic longitude
 * @param elevation    The geodetic elevation
 * @param ecef         The corresponding ECEF coordinates
 *
 * Transform geodetic coordinates to Cartesian ones in the Earth-Centered,
 * Earth-Fixed (ECEF) frame.
 */
TURTLE_API void turtle_ecef_from_geodetic(
    double latitude, double longitude, double elevation, double ecef[3]);

/**
 * Transform Cartesian ECEF coordinates to geodetic ones
 *
 * @param ecef         The ECEF coordinates
 * @param latitude     The corresponding geodetic latitude
 * @param longitude    The corresponding geodetic longitude
 * @param altitude     The corresponding geodetic altitude
 *
 * Transform Cartesian coordinates in the Earth-Centered, Earth-Fixed (ECEF)
 * frame to geodetic ones. B. R. Bowring's (1985) algorithm's is used with a
 * single iteration.
 */
TURTLE_API void turtle_ecef_to_geodetic(const double ecef[3], double * latitude,
    double * longitude, double * altitude);

/**
 * Transform horizontal angles to a Cartesian direction in ECEF
 *
 * @param latitude     The geodetic latitude
 * @param longitude    The geodetic longitude
 * @param azimuth      The geographic azimuth angle
 * @param elevation    The geographic elevation angle
 * @param direction    The corresponding direction in ECEF coordinates
 *
 * Transform horizontal coordinates to a Cartesian direction in the
 * Earth-Centered, Earth-Fixed (ECEF) frame.
 */
TURTLE_API void turtle_ecef_from_horizontal(double latitude, double longitude,
    double azimuth, double elevation, double direction[3]);

/**
 * Transform a Cartesian direction in ECEF to horizontal angles
 *
 * @param latitude     The geodetic latitude
 * @param longitude    The geodetic longitude
 * @param direction    The direction vector in ECEF coordinates
 * @param azimuth      The corresponding geographic azimuth
 * @param elevation    The corresponding geographic elevation
 *
 * Transform a Cartesian direction vector in the Earth-Centered, Earth-Fixed
 * (ECEF) frame to horizontal coordinates.
 */
TURTLE_API void turtle_ecef_to_horizontal(double latitude, double longitude,
    const double direction[3], double * azimuth, double * elevation);

/**
 * Create a new stack of global topography data
 *
 * @param stack         The stack object
 * @param path          The path where elevation data are stored, or `NULL`.
 * @param size          The maximum number of elevation maps kept in memory.
 * @param lock          A callback for locking critical sections, or `NULL`.
 * @param unlock        A callback for unlocking critical sections, or `NULL`.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Allocate memory for a new stack and initialise it. Use `turtle_stack_destroy`
 * in order to recover any memory allocated subsequently. The stack is
 * initialised as empty. **Note** that providing a null or negative stack *size*
 * results in all maps being kept in memory.
 *
 * __Warnings__
 *
 * For multi-threaded access to elevation data, using a `turtle_client` one must
 * provide both a `lock` and `unlock` callback, e.g. based on `sem_wait` and
 * `sem_post`. Otherwise they can be both set to `NULL`. Note that setting only
 * one to not `NULL` raises a `TURTLE_RETURN_BAD_FORMAT` error.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS     The lock and unlock callbacks are
 * inconsistent
 *
 *    TURTLE_RETURN_BAD_FORMAT      The format of the elevation data is not
 * supported
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The stack couldn't be allocated
 */
TURTLE_API enum turtle_return turtle_stack_create(struct turtle_stack ** stack,
    const char * path, int stack_size, turtle_stack_locker_t * lock,
    turtle_stack_locker_t * unlock);

/**
 * Destroy a stack of global topography data
 *
 * @param stack    The stack object
 *
 * Fully destroy a stack and all its allocated elevation data. On return `stack`
 * is set to `NULL`.
 *
 * __Warnings__
 *
 * This method is not thread safe. All clients should have been destroyed or
 * disabled first.
 */
TURTLE_API void turtle_stack_destroy(struct turtle_stack ** stack);

/**
 * Clear the stack from topography data
 *
 * @param stack    The stack object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Clear the stack from any elevation data not currently reserved by a
 * `turtle_client`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released
 */
TURTLE_API enum turtle_return turtle_stack_clear(struct turtle_stack * stack);

/**
 * Load the stack elevation data into memory
 *
 * @param stack    The stack object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Load the stack elevation data into memory, until the max stack size is
 * reached or all tiles have been loaded.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released
 */
TURTLE_API enum turtle_return turtle_stack_load(struct turtle_stack * stack);

/**
 * Get the elevation at geodetic coordinates
 *
 * @param stack        The stack object
 * @param latitude     The geodetic latitude
 * @param longitude    The geodetic longitude
 * @param elevation    The estimated elevation
 * @param inside       Flag for bounds check or `NULL`
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Compute an estimate of the elevation at the given geodetic coordinates. The
 * elevation is linearly interpolated using the 4 nodes that surround the given
 * coordinate. Providing a non `NULL` value for *inside* allows to check if the
 * provided coordinates are inside the stack tiles or not. **Note** that no
 * bound error is raised in the latter case, when *inside* is not `NULL`.
 *
 * __Warnings__ this function is not thread safe. A `turtle_client` must be
 * used instead for concurrent accesses to the stack data.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PATH    The required elevation data are not in the
 * stack path.
 */
TURTLE_API enum turtle_return turtle_stack_elevation(
    struct turtle_stack * stack, double latitude, double longitude,
    double * elevation, int * inside);

/**
 * Create a new client to a stack of global topography data
 *
 * @param client    The client object
 * @param stack     The serving stack
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Allocate memory for a new client for a thread safe access to the elevation
 * data of a stack. The client is initialised as idle. Whenever a new elevation
 * value is requested it will book the needed data to its master `turtle_stack`
 * and release any left over ones. Use `turtle_client_clear`in order to force
 * the release of any reserved data or `turtle_client_destroy` in order to fully
 * recover the client's memory.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS     The stack is not valid, e.g. it has no lock
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The client couldn't be allocated
 */
TURTLE_API enum turtle_return turtle_client_create(
    struct turtle_client ** client, struct turtle_stack * stack);

/**
 * Properly clean a client for a stack
 *
 * @param client    The client object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Attempts to destroy a stack client. Any reserved elevation data are first
 * freed. On a successful return `client` is set to `NULL`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released
 */
TURTLE_API enum turtle_return turtle_client_destroy(
    struct turtle_client ** client);

/**
 * Unbook any reserved elevation data
 *
 * @param client    The client object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Notify the master stack that any previously reserved elevation data is no
 * more needed.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released
 */
TURTLE_API enum turtle_return turtle_client_clear(
    struct turtle_client * client);

/**
 * Thread safe access to the elevation data of a stack
 *
 * @param client       The client object
 * @param latitude     The geodetic latitude
 * @param longitude    The geodetic longitude
 * @param elevation    The estimated elevation
 * @param inside       Flag for bounds check or `NULL`
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Compute an estimate of the elevation at the given geodetic coordinates. The
 * elevation is linearly interpolated using the 4 nodes that surround the given
 * coordinate. Providing a non `NULL` value for *inside* allows to check if the
 * provided coordinates are inside the stack tiles or not. **Note** that no
 * bound error is raised in the latter case, when *inside* is not `NULL`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PATH        The required elevation data are not in the
 * stack path
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released
 */
TURTLE_API enum turtle_return turtle_client_elevation(
    struct turtle_client * client, double latitude, double longitude,
    double * elevation, int * inside);

/**
 * Create a new ECEF stepper
 *
 * @param stepper   The stepper object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Allocate memory for a new stepper providing smart access to geodetic and
 * elevation data given ECEF coordinates. Call `turtle_stepper_destroy`
 * in order to properly recover any allocated memory.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The stepper couldn't be allocated
 */
TURTLE_API enum turtle_return turtle_stepper_create(
    struct turtle_stepper ** stepper);

/**
 * Properly clean an ECEf stepper
 *
 * @param stepper    The stepper object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Attempts to destroy an ECEF stepper. **Note** that the stepper might have
 * created a `turtle_client` for thread safe access to stack data.
 * If so, the client is automatically destroyed as well.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The client lock couldn't be acquired
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The client lock couldn't be released
 */
TURTLE_API enum turtle_return turtle_stepper_destroy(
    struct turtle_stepper ** stepper);

/**
 * Set a geoid model for altitude corrections
 *
 * @param stepper    The stepper object
 * @param geoid      A map object with the geoid undulations
 *
 * Note that by default no geoid undulations corrections are applied. Altitudes
 * are w.r.t. WGS84 ellipsoid, not w.r.t. mean sea level.  For long range
 * stepping this might introduce distortions of the ground since topography data
 * are usually given w.r.t. mean sea level. Providing a geoid map allows to
 * correct for this.
 */
TURTLE_API void turtle_stepper_geoid_set(
    struct turtle_stepper * stepper, struct turtle_map * geoid);

/**
 * Get the current geoid map
 *
 * @param stepper    The stepper object
 * @return A handle to the geoid map
 */
TURTLE_API struct turtle_map * turtle_stepper_geoid_get(
    const struct turtle_stepper * stepper);

/**
 * Set the validity range for local approximation to geographic transforms
 *
 * @param stepper    The stepper object
 * @param range      The approximation range, in m
 *
 * Setting a strictly positive range enables the use of local linear
 * approximations (LLA) to geographic transforms, for small consecutive
 * steps. The default range value is 1 m. Set this to zero in order to
 * disable LLA.
 */
TURTLE_API void turtle_stepper_range_set(
    struct turtle_stepper * stepper, double range);

/**
 * Get the validity range for local approximation to geographic transforms
 *
 * @param stepper    The stepper object
 * @return The approximation range, in m.
 */
TURTLE_API double turtle_stepper_range_get(
    const struct turtle_stepper * stepper);

/**
 * Get the slope factor for the stepping algorithm
 *
 * @param stepper    The stepper object
 * @return The slope factor*
 */
TURTLE_API double turtle_stepper_slope_get(
    const struct turtle_stepper * stepper);

/**
 * Set the slope factor for the stepping algorithm
 *
 * @param stepper    The stepper object
 * @param slope      The slope factor
 *
 * Setting a slope factor smaller than one allows to resolve stepper slopes
 * but at the cost of slowing down the stepping. The default value is 0.4.
 */
TURTLE_API void turtle_stepper_slope_set(
    struct turtle_stepper * stepper, double slope);

/**
 * Get the resolution factor for the stepping algorithm
 *
 * @param stepper    The stepper object
 * @return The resolution factor
 */
TURTLE_API double turtle_stepper_resolution_get(
    const struct turtle_stepper * stepper);

/**
 * Set the resolution factor for the stepping algorithm
 *
 * @param stepper        The stepper object
 * @param resolution     The resolution factor
 *
 * The resolution factor corresponds to the minimum length for the initial
 * step. Its default value is 1E-02 (1 cm).
 */
TURTLE_API void turtle_stepper_resolution_set(
    struct turtle_stepper * stepper, double resolution);

/**
 * Add a new geometry layer for the stepper
 *
 * @param stepper   The stepper object
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Add a new geometry layer. Data can be further added to the layer using the
 * `turtle_stepper_add_*` functions. If the current layer is empty, no new layer
 * is added. At creation, the stepper geometry is initialised with a single
 * empty layer.
 *
 * **Note** that the added registered layer is the top one. But, the lower a
 * layer the higher its priority.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The layer couldn't be allocated.
 */
TURTLE_API enum turtle_return turtle_stepper_add_layer(
    struct turtle_stepper * stepper);

/**
 * Add a `turtle_stack` data resource to the current geometry layer
 *
 * @param stepper   The stepper object
 * @param stack     The stack to access
 * @param offset    Any offset to the elevation data
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Register a `turtle_stack` data resource for the current geometry layer of
 * the stepper. An offset to the native elevation data can be specified as well.
 * Note that if the stack supports multi-threading, i.e. if it has a registered
 * lock, then a `turtle_client` is automatically created in order to access the
 * stack data.
 *
 * **Note** that the last registered data within the current layer is the top
 * data, i.e. it has priority over data beneath.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS     The client could not be created.
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The layer couldn't be allocated.
 */
TURTLE_API enum turtle_return turtle_stepper_add_stack(
    struct turtle_stepper * stepper, struct turtle_stack * stack,
    double offset);

/**
 * Add a `turtle_map` data resource to the current geometry layer
 *
 * @param stepper   The stepper object
 * @param map       The map to access
 * @param offset    Any offset to the elevation data
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Register a `turtle_map` data resource for the current geometry layer of
 * the stepper. An offset to the native elevation data can be specified as well.
 *
 * **Note** that the last registered data within the current layer is the top
 * data, i.e. it has priority over data beneath.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The layer couldn't be allocated
 */
TURTLE_API enum turtle_return turtle_stepper_add_map(
    struct turtle_stepper * stepper, struct turtle_map * map, double offset);

/**
 * Add a flat data resource to the current geometry layer
 *
 * @param stepper   The stepper object
 * @param offset    Any offset to the elevation data
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Register a `flat` (zero elevation) data resource for the current geometry
 * layer of the stepper. An offset can be specified as well.
 *
 * **Note** that the last registered data within the current layer is the top
 * data, i.e. it has priority over data beneath.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The layer couldn't be allocated
 */
TURTLE_API enum turtle_return turtle_stepper_add_flat(
    struct turtle_stepper * stepper, double ground_level);

/**
 * Compute (or do) a step through the topography using ECEF coordinates
 *
 * @param stepper              The stepper object
 * @param position             The initial (final) ECEF position
 * @param direction            The initial direction in ECEF, or `NULL`
 * @param latitude             The (final) geodetic latitude
 * @param longitude            The (final) geodetic longitude
 * @param altitude             The (final) geodetic altitude
 * @param elevation            The (final) topography elevation(s)
 * @param step_length          The step length
 * @param index                The (final) geometry and/or meta-data indices
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * If *direction* is `NULL`, sample the geography data at the given ECEF
 * position and compute a tentative *step length*. The top most valid data in
 * the stepper's stack are returned. If no valid layer was found a negative
 * *layer* value is returned, or an error is raised if *layer* points to `NULL`.
 *
 * If a *direction* is provided, do a single step through the topography along
 * the given direction, using the tentative *step length*. If a change of medium
 * occurs, the boundary is located using a binary search. At exit the ECEF
 * position is updated. If the step exit the topography area and if *layer* is
 * non `NULL`, then a negative layer value is returned. Otherwise an error is
 * raised.
 *
 * Note that any of the output data can point to `NULL` if it is of no interest.
 * Note also that depending of the set local *range*, an approximation might be
 * used for computing geographic coordinates.
 *
 * If the stepper has a single geometry layer, the matching meta data elevation
 * value and index are returned. Else, if non `NULL`,`*elevation* and *index*
 * must be size 2 arrays. Then, the elevation values of the lower (elevation[0])
 * and upper (elevation[1]) bounding layers are returned. The index array is
 * filled with the containing geometry layer (index[0]) and the matching
 * meta-data inside the layer (index[1]).
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR    The provided position is outside of all
 * data
 */
enum turtle_return turtle_stepper_step(struct turtle_stepper * stepper,
    double * position, const double * direction, double * latitude,
    double * longitude, double * altitude, double * elevation,
    double * step, int * index);

/**
 * Convert a geograhic location to an ECEF one
 *
 * @param stepper              The stepper object
 * @param latitude             The geodetic latitude
 * @param longitude            The geodetic longitude
 * @param height               The height w.r.t. the layer elevation
 * @param layer_index          The index of the reference layer
 * @param position             The corresponding ECEF position
 * @param data_index           The index of the matching meta-data
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below
 *
 * Inspect the stepper's geometry layer and provide the top most ECEF position.
 * If no valid meta-data was found a negative *data_index* value is returned,
 * or an error is raised if *data_index* points to `NULL`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR    The provided position is outside of all
 * data
 */
enum turtle_return turtle_stepper_position(struct turtle_stepper * stepper,
    double latitude, double longitude, double height, int layer_index,
    double * position, int * data_index);

#ifdef __cplusplus
}
#endif
#endif

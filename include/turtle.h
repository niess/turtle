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

#ifndef TURTLE_H
#define TURTLE_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return Codes used by TURTLE.
 */
enum turtle_return {
        /** The operation succeeded. */
        TURTLE_RETURN_SUCCESS = 0,
        /** A wrong pointer address was provided, e.g. NULL. */
        TURTLE_RETURN_BAD_ADDRESS,
        /** A provided file extension is not supported or recognised. */
        TURTLE_RETURN_BAD_EXTENSION,
        /** A provided file or string has a wrong format. */
        TURTLE_RETURN_BAD_FORMAT,
        /** A provided `turtle_projection` is not supported. */
        TURTLE_RETURN_BAD_PROJECTION,
        /** Some JSON metadata couldn't be understood. */
        TURTLE_RETURN_BAD_JSON,
        /** Some input parameters are out of their validity range. */
        TURTLE_RETURN_DOMAIN_ERROR,
        /** An TURTLE low level library error occured. */
        TURTLE_RETURN_LIBRARY_ERROR,
        /** A lock couldn't be acquired. */
        TURTLE_RETURN_LOCK_ERROR,
        /** Some meory couldn't be allocated. */
        TURTLE_RETURN_MEMORY_ERROR,
        /** A provided path wasn't found. */
        TURTLE_RETURN_PATH_ERROR,
        /** A lock couldn't be released. */
        TURTLE_RETURN_UNLOCK_ERROR,
        /** The number of TURTLE error codes. */
        N_TURTLE_RETURNS
};

/**
 * Handle for a geographic projection.
 */
struct turtle_projection;

/**
 * Handle for a projection map of the elevation.
 */
struct turtle_map;

/**
 * Handle for a geodetic datum for worldwide elevation data.
 */
struct turtle_datum;

/**
 * Handle to a datum client for multithreaded access to elevation data.
 */
struct turtle_client;

/**
 * Bounding box for projection maps.
 */
struct turtle_box {
        /** Origin's X coordinate. */
        double x0;
        /** Origin's Y coordinate. */
        double y0;
        /** Half width along the X-axis. */
        double half_x;
        /** Half width along the Y-axis. */
        double half_y;
};

/**
 * Generic function pointer.
 *
 * This is a generic function pointer used to identify the library functions,
 * e.g. for error handling.
 */
typedef void turtle_caller_t(void);

/**
 * Callback for error handling.
 *
 * @param rc        The TURTLE return code.
 * @param caller    The caller function where the error occured.
 *
 * The user might provide its own error handler. It will be called at the
 * return of any TURTLE library function providing an error code.
 *
 * __Warnings__
 *
 * This callback *must* be thread safe if a `turtle_client` is used.
 */
typedef void turtle_handler_cb(enum turtle_return rc, turtle_caller_t * caller);

/**
 * Callback for locking or unlocking critical sections.
 *
 * @return `0` on success, any other value otherwise.
 *
 * For multhithreaded applications with a `turtle_datum` and `turtle_client`s
 * the user must supply a `lock` and `unlock` callback providing exclusive
 * access to critical sections, e.g. using a semaphore.
 *
 * __Warnings__
 *
 * The callback *must* return `0` if the (un)lock was successfull.
 */
typedef int turtle_datum_cb(void);

/**
 * Initialise the TURTLE library.
 *
 * @param error_handler    A user supplied error_handler or NULL.
 *
 * Initialise the library. Call `turtle_finalise` in order to unload the
 * library. Optionally, an error handler might be provided.
 *
 * __Warnings__
 *
 * This function is not thread safe.
 */
void turtle_initialise(turtle_handler_cb * handler);

/**
 * Finalise the TURTLE library.
 *
 * Unload the library. `turtle_initialise` must have been called first.
 *
 * __Warnings__
 *
 * This function is not thread safe.
 */
void turtle_finalise(void);

/**
 * Return a string describing a `turtle_return` code.
 *
 * This function is analog to the C89 `strerror` function but specific to
 * TURTLE return codes. It is thread safe.
 */
const char * turtle_strerror(enum turtle_return rc);

/**
 * Return a string describing a TURTLE library function.
 *
 * This function is meant for verbosing when handling errors. It is thread
 * safe.
 */
const char * turtle_strfunc(turtle_caller_t * function);

/**
 * Setter for the library error handler.
 *
 * @param handler    The user supplied error handler.
 *
 * This function allows to set or alter the error handler. Only one error
 * handler can be set at a time for all threads. It is not thread safe
 * to modify it.
 */
void turtle_handler(turtle_handler_cb * handler);

/**
 * Create a new geographic projection.
 *
 * @param name          The name of the projection.
 * @param projection    A handle to the projection.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * The currently supported projections are **Lambert** and **UTM**. The `name`,
 * which encodes the projection parameters, must be as following:
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
 *    TURTLE_RETURN_BAD_PROJECTION    The name isn't valid.
 *
 *    TURTLE_RETURN_MEMORY_ERROR      The projection couldnt be allocated.
 */
enum turtle_return turtle_projection_create(
    const char * name, struct turtle_projection ** projection);

/**
 * Destroy a geographic projection.
 *
 * @param projection    A handle to the projection.
 *
 * Fully destroy a geographic projection previously allocated with
 * `turtle_projection_create`. On return `projection` is set to `NULL`.
 *
 * __Warnings__
 *
 * This must **not** be called on a projection handle returned by a
 * `turtle_map`. Instead one must call `turtle_map_destroy` to get rid of
 * both the map and its projection.
 */
void turtle_projection_destroy(struct turtle_projection ** projection);

/**
 * (Re-)configure a geographic projection.
 *
 * @param name          The name of the projection.
 * @param projection    A handle to the projection.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * See `turtle_projection_create` for the supported projections and valid
 * values for the `name` parameter.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The projection isn't supported.
 */
enum turtle_return turtle_projection_configure(
    const char * name, struct turtle_projection * projection);

/**
 * Information on a geographic projection.
 *
 * @param projection    A handle to the projection.
 * @param name          The name of the projection.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * This function fills in the provided `name` string with the projection
 * details. The resulting name conforms to the inputs to
 * `turtle_projection_configure` or `turtle_projection_create`, e.g. `UTM 31N`.
 * See the later for a detailed description.
 *
 * __Warnings__
 *
 * This function allocates the `name` string. It is the user's responsability
 * to free it when no more needed.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The projection isn't supported.
 *
 *    TURTLE_RETURN_MEMORY_ERROR      The name string couldn't be allocated.
 */
enum turtle_return turtle_projection_info(
    const struct turtle_projection * projection, char ** name);

/**
 * Apply a geographic projection to geodetic coordinates.
 *
 * @param projection    A handle to the projection.
 * @param latitude      The input geodetic latitude.
 * @param latitude      The input geodetic longitude.
 * @param x             The output X-coordinate.
 * @param y             The output Y-coordinate.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Apply the geographic projection to a set of geodetic coordinates. See
 * `turtle_projection_unproject` for the reverse transform.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS       The projection is `NULL`.
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The projection isn't supported.
 */
enum turtle_return turtle_projection_project(
    const struct turtle_projection * projection, double latitude,
    double longitude, double * x, double * y);

/**
 * Unfold a geographic projection to recover the geodetic coordinates.
 *
 * @param projection    A handle to the projection.
 * @param x             The input X-coordinate.
 * @param y             The input Y-coordinate.
 * @param latitude      The output geodetic latitude.
 * @param latitude      The output geodetic longitude.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Unfold a geographic projection to recover the initial geodetic coordinates.
 * See `turtle_projection_project` for the direct transform.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS       The projection is `NULL`.
 *
 *    TURTLE_RETURN_BAD_PROJECTION    The provided projection isn't supported.
 */
enum turtle_return turtle_projection_unproject(
    const struct turtle_projection * projection, double x, double y,
    double * latitude, double * longitude);

/**
 * Create a new projection map.
 *
 * @param projection    The name of the projection or `NULL`.
 * @param box           A bouding box for the map.
 * @param nx            The number of nodes along the X-axis.
 * @param ny            The number of nodes along the Y-axis.
 * @param zmin          The minimum elevation value.
 * @param zmax          The maximum elevation value.
 * @param bit_depth     The number of bits for storing elevation values.
 * @param map           A handle to the map.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Allocate memory for a new projection map and initialise it. Use
 * `turtle_map_destroy` in order to recover the allocated memory. The map is
 * initialised as flat with `nx x ny` static nodes of elevation `zmin`. The
 * nodes are distributed over a regular grid defined by `box`. The elevation
 * values are stored over `bit_depth` bits between `zmin` and `zmax`. If
 * `projection` is not `NULL` the map is initialised with a geographic
 * projection handle. See `turtle_projection_create` for a list of valid
 * projections and their names.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PROJECTION    An invalid projection was provided.
 *
 *    TURTLE_RETURN_DOMAIN_ERROR      An input parameter is out of its
 * validity range.
 *
 *    TURTLE_RETURN_MEMORY_ERROR      The map couldn't be allocated.
 */
enum turtle_return turtle_map_create(const char * projection,
    const struct turtle_box * box, int nx, int ny, double zmin, double zmax,
    int bit_depth, struct turtle_map ** map);

/**
 * Destroy a projection map.
 *
 * @param map    A handle to the map.
 *
 * Fully destroy a projection map and recover the memory previously allocated
 * by `turtle_projection_create`. On return `map` is set to `NULL`.
 */
void turtle_map_destroy(struct turtle_map ** map);

/**
 * Load a projection map.
 *
 * @param path    The path to the map file.
 * @param box     A bouding box for the map or `NULL`.
 * @param map     A handle to the map.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Load a projection map from a file. The file format is guessed from the
 * filename's extension. Currently only a custom `.png` format is supported.
 * A bouding `box` can be provided in order to load only a rectangular subset
 * of the initial map. The bounding box coordinates must be specified in the
 * initial map frame.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_EXTENSION    The file format is not supported.
 *
 *    TURTLE_RETURN_BAD_PATH         The file wasn't found.
 *
 *    TURTLE_RETURN_DOMAIN_ERROR     The bouding box is invalid.
 *
 *    TURTLE_RETURN_MEMORY_ERROR     The map couldn't be allocated.
 *
 *    TURTLE_RETURN_JSON_ERROR       The JSON metadata are invalid (.png file).
 *
 */
enum turtle_return turtle_map_load(
    const char * path, const struct turtle_box * box, struct turtle_map ** map);

/**
 * Dump a projection map to a file.
 *
 * @param map     A handle to the map.
 * @param path    The path for the output file.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Dump a projection map to a file. The file format is guessed from the output
 * filename's extension. Currently only a custom `.png` format is supported.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_EXTENSION    The file format is not supported.
 *
 *    TURTLE_RETURN_BAD_FORMAT       The map cannot be dumped in the given
 * format, e.g. 16 bit.
 *
 *    TURTLE_RETURN_BAD_PATH         The file couldn't be opened.
 *
 *    TURTLE_RETURN_MEMORY_ERROR     Some temporary memory couldn't be
 * allocated.
 */
enum turtle_return turtle_map_dump(
    const struct turtle_map * map, const char * path);

/**
 * Fill the elevation value of a map node.
 *
 * @param map          A handle to the map.
 * @param ix           The node X-coordinate.
 * @param iy           The node Y-coordinate.
 * @param elevation    The elevation value to set.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Fill the elevation value of the map node of coordinates `(ix, iy)`. The
 * elevation value must be in the range `[zmin, zmax]` of the `map`. **Note**
 * that depending on the map's `bit_depth` the actual node elevation can
 * differ from the input value by `(zmax-zmin)/(2**(bit_depth)-1)`, e.g 1.5 cm
 * for a 1 km altitude span and a 16 bit resolution.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR    Some input parameter isn't valid.
 */
enum turtle_return turtle_map_fill(
    struct turtle_map * map, int ix, int iy, double elevation);

/**
 * Get the properties of a map node.
 *
 * @param map          A handle to the map.
 * @param ix           The node X-coordinate.
 * @param iy           The node Y-coordinate.
 * @param x            The node geographic X-coordinate.
 * @param y            The node geographic Y-coordinate.
 * @param elevation    The node elevation value.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise
 * `TURTLE_RETURN_DOMAIN_ERROR` if any input parameter isn't valid.
 *
 * Get the properties of a map node, i.e. its geographic coordinates and
 * elevation value.
 */
enum turtle_return turtle_map_node(struct turtle_map * map, int ix, int iy,
    double * x, double * y, double * elevation);

/**
 * Get the map elevation at a geographic coordinate.
 *
 * @param map          A handle to the map.
 * @param x            The geographic X-coordinate.
 * @param y            The geographic Y-coordinate.
 * @param elevation    The elevation value.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Compute an estimate of the map elevation at the given geographic
 * coordinates. The elevation is linearly interpolated using the 4 nodes that
 * surround the given coordinate.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR    The coordinates are not valid.
 */
enum turtle_return turtle_map_elevation(
    const struct turtle_map * map, double x, double y, double * elevation);

/**
 * Get a handle to the map's projection.
 *
 * @param map    A handle to the map.
 * @return A handle to the map's projection or `NULL` if the map is `NULL`.
 *
 * **Note** The provided projection handle allows to set and modify the map's
 * projection, e.g. using `turtle_projection_configure`.
 */
struct turtle_projection * turtle_map_projection(struct turtle_map * map);

/**
 * Get basic information on a projection map.
 *
 * @param map          A handle to the map.
 * @param box          The map bounding box in X-Y.
 * @param nx           The number of nodes along the X-axis.
 * @param ny           The number of nodes along the Y-axis.
 * @param zmin         The minimum allowed elevation value.
 * @param zmax         The maximum allowed elevation value.
 * @param bit_depth    The number of bits for storing elevation values.
 *
 * Get some basic information on a projection map. Note that any output
 * parameter can be set to NULL if the corresponding property is not needed.
 */
void turtle_map_info(const struct turtle_map * map, struct turtle_box * box,
    int * nx, int * ny, double * zmin, double * zmax, int * bit_depth);

/**
 * Create a new geodetic datum.
 *
 * @param path          The path where elevation data are stored.
 * @param stack_size    The number of elevation data tiles kept in memory.
 * @param lock          A callback for locking critical sections, or `NULL`.
 * @param unlock        A callback for unlocking critical sections, or `NULL`.
 * @param datum         A handle to the datum.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Allocate memory for a new geodetic datum and initialise it. Use
 * `turtle_datum_destroy` in order to recover any memory allocated subsequently.
 * The datum is initialised with an empty stack. The name of the path folder
 * encodes the data source as detailed below. It doesn't need to actually
 * store elevation data, e.g. if only geographic transforms are needed.
 *
 * __Warnings__
 *
 * For multithreaded access to elevation data, using a `turtle_client` one must
 * provide both a `lock` and `unlock` callback, e.g. based on `sem_wait` and
 * `sem_post`. Otherwise they can be both set to `NULL`. Note that setting only
 * one to not `NULL` raises a `TURTLE_RETURN_BAD_FORMAT` error.
 *
 * __Supported sources__
 *
 *    ASTGTM2    ASTER-GDEM2 data, e.g. from http://reverb.echo.nasa.gov/reverb.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS     The lock and unlock callbacks are
 * inconsistent.
 *
 *    TURTLE_RETURN_BAD_FORMAT      The format of the elevation data is not
 * supported.
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The datum couldn't be allocated.
 */
enum turtle_return turtle_datum_create(const char * path, int stack_size,
    turtle_datum_cb * lock, turtle_datum_cb * unlock,
    struct turtle_datum ** datum);

/**
 * Destroy a gedodetic datum.
 *
 * @param datum    A handle to the datum.
 *
 * Fully destroy a datum and all its allocated elevation data. On return
 * `datum` is set to `NULL`.
 *
 * __Warnings__
 *
 * This method is not thread safe. All clients should have been destroyed or
 * disabled first.
 */
void turtle_datum_destroy(struct turtle_datum ** datum);

/**
 * Clear the stack of elevation data.
 *
 * @param datum    A handle to the datum.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Clear the stack from any elevation data not currently reserved by a
 * `turtle_client`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired.
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released.
 */
enum turtle_return turtle_datum_clear(struct turtle_datum * datum);

/**
 * Get the elevation at geodetic coordinates.
 *
 * @param datum        A handle to the datum.
 * @param latitude     The geodetic latitude.
 * @param longitude    The geodetic longitude.
 * @param elevation    The estimated elevation.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Compute an estimate of the elevation at the given geodetic coordinates. The
 * elevation is linearly interpolated using the 4 nodes that surround the given
 * coordinate.
 *
 * __Warnings__ this function is not thread safe. A `turtle_client` must be
 * used instead for concurrent accesses to the datum elevation data.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PATH    The required elevation data are not in the
 * datum's path.
 */
enum turtle_return turtle_datum_elevation(struct turtle_datum * datum,
    double latitude, double longitude, double * elevation);

/**
 * Transform geodetic coordinates to cartesian ECEF ones.
 *
 * @param datum        A handle to the datum.
 * @param latitude     The geodetic latitude.
 * @param longitude    The geodetic longitude.
 * @param elevation    The geodetic elevation.
 * @param ecef         The corresponding ECEF coordinates.
 *
 * Transform geodetic coordinates to Cartesian ones in the Earth-Centered,
 * Earth-Fixed (ECEF) frame.
 */
void turtle_datum_ecef(struct turtle_datum * datum,
    double latitude, double longitude, double elevation, double ecef[3]);

/**
 * Transform cartesian ECEF coordinates to geodetic ones.
 *
 * @param datum        A handle to the datum.
 * @param ecef         The ECEF coordinates.
 * @param latitude     The corresponding geodetic latitude.
 * @param longitude    The corresponding geodetic longitude.
 * @param elevation    The corresponding geodetic elevation.
 *
 * Transform Cartesian coordinates in the Earth-Centered, Earth-Fixed (ECEF)
 * frame to geodetic ones. B. R. Bowring's (1985) algorithm's is used with a
 * single iteration.
 */
void turtle_datum_geodetic(struct turtle_datum * datum,
    double ecef[3], double * latitude, double * longitude, double * elevation);

/**
 * Transform horizontal coorrdinates to a cartesian direction in ECEF.
 *
 * @param datum        A handle to the datum.
 * @param latitude     The geodetic latitude.
 * @param longitude    The geodetic longitude.
 * @param azimuth      The geographic azimuth angle.
 * @param elevation    The geographic elevation angle.
 * @param direction    The corresponding direction in ECEF coordinates.
 *
 * Transform horizontal coordinates to a Cartesian direction in the
 * Earth-Centered, Earth-Fixed (ECEF) frame.
 */
void turtle_datum_direction(struct turtle_datum * datum,
    double latitude, double longitude, double azimuth, double elevation,
    double direction[3]);

/**
 * Transform a cartesian direction in ECEF to horizontal coorrdinates.
 *
 * @param datum        A handle to the datum.
 * @param latitude     The geodetic latitude.
 * @param longitude    The geodetic longitude.
 * @param direction    The direction vector in ECEF coordinates.
 * @param azimuth      The corresponding geographic azimuth.
 * @param elevation    The corresponding geographic elevation.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Transform a Cartesian direction vector in the Earth-Centered, Earth-Fixed
 * (ECEF) frame to horizontal coordinates.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_DOMAIN_ERROR   The direction has a null norm.
 */
enum turtle_return turtle_datum_horizontal(struct turtle_datum * datum,
    double latitude, double longitude, double direction[3], double * azimuth,
    double * elevation);

/**
 * Create a new client for a geodetic datum.
 *
 * @param datum     The master geodetic datum.
 * @param client    A handle to the client.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Allocate memory for a new client for a thread safe access to the elevation
 * data of a geodetic datum. The client is initialised as iddle. Whenever a new
 * elevation value is requested it will book the needed data to its master
 * `turtle_datum` and release any left over ones. Use `turtle_client_clear`in
 * order to force the release of any reserved data or `turtle_client_destroy`
 * in order to fully recover the client's memory.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_ADDRESS     The datum is not valid, e.g. has no lock.
 *
 *    TURTLE_RETURN_MEMORY_ERROR    The client couldn't be allocated.
 */
enum turtle_return turtle_client_create(
    struct turtle_datum * datum, struct turtle_client ** client);

/**
 * Create a new client for a geodetic datum.
 *
 * @param client    A handle to the client.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Attempts to destroy a datum client. Any reserved elevation data are first
 * freed. On a successfull return `client` is set to `NULL`.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired.
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released.
 */
enum turtle_return turtle_client_destroy(struct turtle_client ** client);

/**
 * Unbook any reserved elevation data.
 *
 * @param client    A handle to the client.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Notify the master datum that any previously reserved elevation data is no
 * more needed.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired.
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released.
 */
enum turtle_return turtle_client_clear(struct turtle_client * client);

/**
 * Thread safe access to the elevation data of a geodetic datum.
 *
 * @param datum        A handle to the client.
 * @param latitude     The geodetic latitude.
 * @param longitude    The geodetic longitude.
 * @param elevation    The estimated elevation.
 * @return On success `TURTLE_RETURN_SUCCESS` is returned otherwise an error
 * code is returned as detailed below.
 *
 * Compute an estimate of the elevation at the given geodetic coordinates. The
 * elevation is linearly interpolated using the 4 nodes that surround the given
 * coordinate.
 *
 * __Error codes__
 *
 *    TURTLE_RETURN_BAD_PATH        The required elevation data are not in the
 * datum's path.
 *
 *    TURTLE_RETURN_LOCK_ERROR      The lock couldn't be acquired.
 *
 *    TURTLE_RETURN_UNLOCK_ERROR    The lock couldn't be released.
 */
enum turtle_return turtle_client_elevation(struct turtle_client * client,
    double latitude, double longitude, double * elevation);

#ifdef __cplusplus
}
#endif
#endif

/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#ifndef TURTLE_H
#define TURTLE_H
#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
enum turtle_return {
	TURTLE_RETURN_SUCCESS = 0,
	TURTLE_RETURN_BAD_ADDRESS,
	TURTLE_RETURN_BAD_EXTENSION,
	TURTLE_RETURN_BAD_FORMAT,
	TURTLE_RETURN_BAD_PATH,
	TURTLE_RETURN_BAD_PROJECTION,
	TURTLE_RETURN_BAD_XML,
	TURTLE_RETURN_DOMAIN_ERROR,
	TURTLE_RETURN_GEOTIFF_ERROR,
	TURTLE_RETURN_MEMORY_ERROR,
	N_TURTLE_RETURNS
};

/* Opaque structures. */
struct turtle_projection;
struct turtle_map;
struct turtle_datum;
struct turtle_client;

/* Bounding box for a projection map. */
struct turtle_box {
	double x0; /* Origin X coordinate or longitude. */
	double y0; /* Origin Y coordinate or latitude. */
	double half_x;
	double half_y;
};

/* User supplied callbacks for locking or unlocking critical sections for
 * multi-threaded applications.
 */
typedef void (* turtle_datum_cb)(void);

/* General library routines. */
enum turtle_return turtle_initialise(void);
void turtle_finalise(void);
const char * turtle_strerror(enum turtle_return rc);

/* Routines for handling geographic projections. */
enum turtle_return turtle_projection_create(const char * name,
	struct turtle_projection ** projection);
void turtle_projection_destroy(struct turtle_projection ** projection);
enum turtle_return turtle_projection_configure(const char * name,
	struct turtle_projection * projection);
enum turtle_return turtle_projection_info(
	const struct turtle_projection * projection, char ** name);
enum turtle_return turtle_projection_project(
	const struct turtle_projection * projection, double latitude,
	double longitude, double * x, double * y);
enum turtle_return turtle_projection_unproject(
	const struct turtle_projection * projection,
	double x, double y, double * latitude, double * longitude);

/* Routines for handling projection maps. */
enum turtle_return turtle_map_load(const char * path,
	const struct turtle_box * box, struct turtle_map ** map);
void turtle_map_destroy(struct turtle_map ** map);
enum turtle_return turtle_map_dump(const struct turtle_map * map,
	const char * path);
enum turtle_return turtle_map_elevation(const struct turtle_map * map,
	double x, double y, double * z);
const struct turtle_projection * turtle_map_projection(
	const struct turtle_map * map);
enum turtle_return turtle_map_info(const struct turtle_map * map,
	struct turtle_box * box, double * zmin, double * zmax);

/* Routines for managing a geodetic datum. */
struct turtle_datum * turtle_datum_create(const char * path,
	int stack_size, turtle_datum_cb lock, turtle_datum_cb release);
void turtle_datum_destroy(struct turtle_datum ** datum);
struct turtle_map * turtle_datum_project(struct turtle_datum * datum,
	const char * projection, const struct turtle_box * box);
void turtle_datum_prune(struct turtle_datum * datum, double timeout);
double turtle_datum_elevation(const struct turtle_datum * datum,
	double latitude, double longitude);
int turtle_datum_ecef(const struct turtle_datum * datum,
	double latitude, double longitude, double position[3]);

/* Routines for accessing a datum using a thread-safe client. */
struct turtle_client * turtle_client_create(struct turtle_datum * datum);
void turtle_client_destroy(struct turtle_client ** client);
struct turtle_map * turtle_client_project(struct turtle_client * client,
	const char * projection, const struct turtle_box * box);
double turtle_client_elevation(const struct turtle_client * client,
	double latitude, double longitude);
int turtle_client_ecef(const struct turtle_client * client,
	double latitude, double longitude, double position[3]);

#ifdef __cplusplus
}
#endif
#endif

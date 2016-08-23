/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#ifndef TURTLE_H
#define TURTLE_H
#ifdef __cplusplus
extern "C" {
#endif

/* Bounding box of a map. */
struct turtle_box {
	double x0; /* Origin X coordinate or longitude. */
	double y0; /* Origin Y coordinate or latitude. */
	double half_x;
	double half_y;
};

int turtle_initialise(void);
void turtle_finalise(void);

/* Routines for handling maps. */
struct turtle_map * turtle_create(const char * name, const int nx, const int ny);
struct turtle_map * turtle_load(const char * path, const struct turtle_box * box);
int turtle_dump(const struct turtle_map * map, const char * path, int bit_depth);
void turtle_destroy(struct turtle_map ** map);

struct turtle_map * turtle_get(const char * path, const struct turtle_box * box);
struct turtle_map * turtle_unused(void);

void turtle_book(struct turtle_map * map);
int turtle_checkout(struct turtle_map * map);

double turtle_elevation(const struct turtle_map * map, double x, double y);
int turtle_geoposition(const struct turtle_map * map, double x, double y,
	double * longitude, double * latitude);
int turtle_locals(const struct turtle_map * map, double longitude,
	double latitude, double * x, double * y);
void turtle_info(const struct turtle_map * map, struct turtle_box * box,
	double * zmin, double * zmax);

#ifdef __cplusplus
}
#endif
#endif

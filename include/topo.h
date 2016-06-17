/*
 * Topographic elevation provider.
 */

#ifndef TOPOGRAPHER_H
#define TOPOGRAPHER_H
#ifdef __cplusplus
extern "C" {
#endif

/* Bounding box of a map. */
struct topo_box {
	double x0; /* Origin X coordinate or longitude. */
	double y0; /* Origin Y coordinate or latitude. */
	double half_x;
	double half_y;
};

int topo_initialise(void);
void topo_finalise(void);

/* Routines for handling maps. */
struct topo_map * topo_create(const char * name, const int nx, const int ny);
struct topo_map * topo_load(const char * path, const struct topo_box * box);
int topo_dump(const struct topo_map * map, const char * path, int bit_depth);
void topo_destroy(struct topo_map ** map);

struct topo_map * topo_get(const char * path, const struct topo_box * box);
struct topo_map * topo_unused(void);

void topo_book(struct topo_map * map);
int topo_checkout(struct topo_map * map);

double topo_elevation(const struct topo_map * map, double x, double y);
int topo_geoposition(const struct topo_map * map, double x, double y,
	double * longitude, double * latitude);
int topo_locals(const struct topo_map * map, double longitude,
	double latitude, double * x, double * y);
void topo_info(const struct topo_map * map, struct topo_box * box);

#ifdef __cplusplus
}
#endif
#endif

/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "turtle.h"
#include "turtle_datum.h"
#include "geotiff16.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* Low level load function(s) for tiles. */
static enum turtle_return load_gdem2(const char * path,
	struct datum_tile ** tile);

/* Create a new datum. */
enum turtle_return turtle_datum_create(const char * path, int stack_size,
	turtle_datum_cb * lock, turtle_datum_cb * unlock,
	struct turtle_datum ** datum)
{
	*datum = NULL;

	/* Check the lock and unlock consistency. */
	if (((lock == NULL) && (unlock != NULL)) ||
		((unlock == NULL) && (lock != NULL)))
		return TURTLE_RETURN_BAD_ADDRESS;

	/* Check the data format. */
	enum datum_format format = DATUM_FORMAT_NONE;
	const char * basename = strrchr(path, '/');
	if (basename == NULL) basename = path;
	else basename++;

	static const char * format_str[N_DATUM_FORMATS] = {"ASTGTM2"};
	static const int buffer_size[N_DATUM_FORMATS] = {32};

	int i;
	for (i = 0; i < N_DATUM_FORMATS; i++) {
		if (strcmp(basename, format_str[i]) == 0) {
			format = i;
			break;
		}
	}
	if (format == DATUM_FORMAT_NONE) return TURTLE_RETURN_BAD_FORMAT;

	/* Allocate the new datum handle. */
	int n = strlen(path)+1;
	*datum = malloc(sizeof(**datum)+n+buffer_size[format]);
	if (*datum == NULL) return TURTLE_RETURN_MEMORY_ERROR;

	/* Initialise the handle. */
	(*datum)->stack = NULL;
	(*datum)->stack_size = 0;
	(*datum)->max_size = stack_size;
	(*datum)->format = format;
	(*datum)->lock = lock;
	(*datum)->unlock = unlock;
	memcpy((*datum)->path, path, n);
	if ((*datum)->path[n-2] != '/')
		(*datum)->path[n-1] = '/';
	(*datum)->buffer = (*datum)->path+n;

	return TURTLE_RETURN_SUCCESS;
}

/* Destroy a datum and all its loaded tiles. */
enum turtle_return turtle_datum_destroy(struct turtle_datum ** datum)
{
	if ((datum == NULL) || (*datum == NULL))
		return TURTLE_RETURN_SUCCESS;

	/* Clean the stack. */
	enum turtle_return rc = turtle_datum_clear(*datum);
	if (rc != TURTLE_RETURN_SUCCESS) return rc;
	if ((*datum)->stack_size > 0)
		return TURTLE_RETURN_MEMORY_ERROR;

	/* Delete the datum and return. */
	free(*datum);
	*datum = NULL;

	return TURTLE_RETURN_SUCCESS;
}

/* Clear the datum's stack from unused tiles. */
enum turtle_return turtle_datum_clear(struct turtle_datum * datum)
{
	if ((datum->lock != NULL) && (datum->lock() != 0))
		return TURTLE_RETURN_LOCK_ERROR;

	struct datum_tile * tile = datum->stack;
	while (tile != NULL) {
		struct datum_tile * current = tile;
		tile = tile->prev;
		if (current->clients == 0)
			datum_tile_destroy(datum, current);
	}

	if ((datum->unlock != NULL) && (datum->unlock() != 0))
		return TURTLE_RETURN_UNLOCK_ERROR;
	else
		return TURTLE_RETURN_SUCCESS;
}

/* Get the datum elevation at the given geodetic coordinates. */
enum turtle_return turtle_datum_elevation(struct turtle_datum * datum,
	double latitude, double longitude, double * elevation)
{
	/* Get the proper tile. */
	double hx, hy;
	int load = 1;
	if (datum->stack != NULL) {
		/* First let's check the top of the stack. */
		struct datum_tile * stack = datum->stack;
		hx = (longitude-stack->x0)/stack->dx;
		hy = (latitude-stack->y0)/stack->dy;

		if ((hx < 0.) || (hx > stack->nx) || (hy < 0.) ||
			(hy > stack->ny)) {
			/* The requested coordinates are not in the top
			 * tile. Let's check the full stack. */
			struct datum_tile * tile = stack->prev;
			while (tile != NULL) {
				hx = (longitude-tile->x0)/tile->dx;
				hy = (latitude-tile->y0)/tile->dy;

				if ((hx >= 0.) && (hx <= tile->nx) &&
					(hy >= 0.) && (hy <= tile->ny)) {
					/*
					 * Move the valid tile to the top
					 * of the stack.
					 */
					datum_tile_touch(datum, tile);
					load = 0;
					break;
				}
				tile = tile->prev;
			}
		}
		else
			load = 0;
	}

	if (load) {
		/* No valid tile was found. Let's try to load it. */
		enum turtle_return rc = datum_tile_load(datum, latitude,
			longitude);
		if (rc != TURTLE_RETURN_SUCCESS) return rc;

		struct datum_tile * stack = datum->stack;
		hx = (longitude-stack->x0)/stack->dx;
		hy = (latitude-stack->y0)/stack->dy;
	}

	/* Interpolate the elevation. */
	int ix = (int)hx;
	int iy = (int)hy;
	hx -= ix;
	hy -= iy;
	const int nx = datum->stack->nx;
	const int ny = datum->stack->ny;
	if (ix < 0) ix = 0;
	if (iy < 0) iy = 0;
	int ix1 = (ix >= nx-1) ? nx-1 : ix+1;
	int iy1 = (iy >= ny-1) ? ny-1 : iy+1;
 	const int16_t * zm = datum->stack->z;
	*elevation = zm[iy*nx+ix]*(1.-hx)*(1.-hy)+zm[iy1*nx+ix]*(1.-hx)*hy+
		zm[iy*nx+ix1]*hx*(1.-hy)+zm[iy1*nx+ix1]*hx*hy;
	return TURTLE_RETURN_SUCCESS;
}

/* Get the parameters of the reference ellipsoid. */
static enum turtle_return get_ellipsoid(enum datum_format format, double * a,
	double * e)
{
	static const double A[N_DATUM_FORMATS] = {6378137.0};
	static const double E[N_DATUM_FORMATS] = {0.081819190842622};

	if ((format < 0) || (format >= N_DATUM_FORMATS))
		return TURTLE_RETURN_BAD_FORMAT;

	*a = A[format];
	*e = E[format];

	return TURTLE_RETURN_SUCCESS;
}

/* Compute ECEF coordinates from latitude and longitude. */
enum turtle_return turtle_datum_ecef(struct turtle_datum * datum,
	double latitude, double longitude, double elevation, double ecef[3])
{
	/* Get the parameters of the reference ellipsoid. */
	double a, e;
	enum turtle_return rc = get_ellipsoid(datum->format, &a, &e);
	if (rc != TURTLE_RETURN_SUCCESS) return rc;

	/* Compute the Cartesian coordinates. */
	const double s = sin(latitude*M_PI/180.);
	const double c = cos(latitude*M_PI/180.);
	const double R = a/sqrt(1.-e*e*s*s);

	ecef[0] = (R+elevation)*c*cos(longitude*M_PI/180.);
	ecef[1] = (R+elevation)*c*sin(longitude*M_PI/180.);
	ecef[2] = (R*(1.-e*e)+elevation)*s;

	return TURTLE_RETURN_SUCCESS;
}

/* Compute the geodetic coordinates from the ECEF ones.
 *
 * Reference: B. R. Bowring's 1985 algorithm (single iteration).
 */
enum turtle_return turtle_datum_geodetic(struct turtle_datum * datum,
	double ecef[3], double * latitude, double * longitude,
	double * elevation)
{
	/* Get the parameters of the reference ellipsoid. */
	double a, e;
	enum turtle_return rc = get_ellipsoid(datum->format, &a, &e);
	if (rc != TURTLE_RETURN_SUCCESS) return rc;
	const double b2 = a*a*(1.-e*e);
	const double b = sqrt(b2);
	const double eb2 = e*e*a*a/b2;

	/* Compute the geodetic coordinates. */
	if ((ecef[0] == 0.) && (ecef[1] == 0.)) {
		*latitude = (ecef[2] >= 0.) ? 90. : -90.;
		*longitude = 0.0;
		*elevation = fabs(ecef[2])-b;
		return TURTLE_RETURN_SUCCESS;
	}

	*longitude = atan2(ecef[1], ecef[0])*180./M_PI;

	const double p2 = ecef[0]*ecef[0]+ecef[1]*ecef[1];
	const double p = sqrt(p2);
	if (ecef[2] == 0.) {
		*latitude = 0.;
		*elevation = p-a;
		return TURTLE_RETURN_SUCCESS;
	}

	const double r = sqrt(p2+ecef[2]*ecef[2]);
	const double tu = b*ecef[2]*(1.+eb2*b/r)/(a*p);
	const double tu2 = tu*tu;
	const double cu = 1./sqrt(1.+tu2);
	const double su = cu*tu;
	const double tp = (ecef[2]+eb2*b*su*su*su)/(p-e*e*a*cu*cu*cu);
	*latitude = atan(tp)*180./M_PI;
	const double cp = 1./sqrt(1.0+tp*tp);
	const double sp = cp*tp;
	*elevation = p*cp+ecef[2]*sp-a*sqrt(1.-e*e*sp*sp);

	return TURTLE_RETURN_SUCCESS;
}

/* Move a tile to the top of the stack. */
void datum_tile_touch(struct turtle_datum * datum, struct datum_tile * tile)
{
	if (tile->next == NULL) return; /* Already on top. */
	struct datum_tile * prev = tile->prev;
	tile->next->prev = prev;
	if (prev != NULL) prev->next = tile->next;
	datum->stack->next = tile;
	tile->prev = datum->stack;
	tile->next = NULL;
	datum->stack = tile;
}

/* Remove a tile from the stack. */
void datum_tile_destroy(struct turtle_datum * datum, struct datum_tile * tile)
{
	struct datum_tile * prev = tile->prev;
	struct datum_tile * next = tile->next;
	if (prev != NULL) prev->next = next;
	if (next != NULL) next->prev = prev;
	else datum->stack = prev;
	free(tile);
	datum->stack_size--;
}

/* Load a new tile and manage the stack. */
enum turtle_return datum_tile_load(struct turtle_datum * datum, int latitude,
	int longitude) {
	if (datum->format == DATUM_FORMAT_ASTER_GDEM2) {
		/* Format the path. */
		const int absL = abs(latitude);
		const int absl = abs(longitude);
		if ((absl > 180) || (absL > 89))
			return TURTLE_RETURN_DOMAIN_ERROR;
		char cL, cl;
		cL = (latitude >= 0) ? 'N' : 'S';
		cl = (longitude >= 0) ? 'E' : 'W';
		sprintf(datum->buffer,
			"ASTGTM2_%c%02d%c%03d_dem.tif", cL, absL, cl, absl);

		/* Load the tile data. */
		struct datum_tile * tile;
		enum turtle_return rc = load_gdem2(datum->path, &tile);
		if (rc != TURTLE_RETURN_SUCCESS) return rc;

		/* Initialise the new tile. */
		tile->clients = 0;
		tile->x0 = longitude;
		tile->y0 = latitude;
		tile->dx = (tile->nx > 1) ? 1./(tile->nx-1) : 0.;
		tile->dy = (tile->ny > 1) ? 1./(tile->ny-1) : 0.;

		/* Make room for the new tile, if needed. */
		if (datum->stack_size >= datum->max_size) {
			struct datum_tile * last = datum->stack;
			while (last->prev != NULL)
				last = last->prev;
			while ((last != NULL) && (datum->stack_size >=
				datum->max_size)) {
				struct datum_tile * current = last;
				last = last->next;
				if (current->clients == 0)
					datum_tile_destroy(datum, current);
			}
		}

		/* Append the new tile on top of the stack. */
		tile->next = NULL;
		tile->prev = datum->stack;
		if (datum->stack != NULL)
			datum->stack->next = tile;
		datum->stack = tile;
		datum->stack_size++;
	}
	else {
		return TURTLE_RETURN_BAD_FORMAT;
	}

	return TURTLE_RETURN_SUCCESS;
}

/* Load ASTER-GDEM2 data to a tile. */
static enum turtle_return load_gdem2(const char * path,
	struct datum_tile ** tile)
{
	struct geotiff16_reader reader;
	enum turtle_return rc = TURTLE_RETURN_SUCCESS;
	*tile = NULL;

	/* Open the geotiff16 file. */
	if (geotiff16_open(path, &reader) != 0)
		return TURTLE_RETURN_BAD_PATH;

	/* Allocate the map. */
	*tile = malloc(sizeof(**tile)+
		sizeof(int16_t)*reader.width*reader.height);
	if (*tile == NULL) {
		rc = TURTLE_RETURN_MEMORY_ERROR;
		goto clean_and_exit;
	}

	/* Copy the tile data. */
	(*tile)->nx = reader.width;
	(*tile)->ny = reader.height;
	if (geotiff16_readinto(&reader, (*tile)->z) != 0)
		rc = TURTLE_RETURN_BAD_FORMAT;

clean_and_exit:
	geotiff16_close(&reader);
	return rc;
}

/*
 *  Topographic Utilities for Rendering The eLEvation (TURTLE)
 *  Copyright (C) 2016  Valentin Niess
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Turtle projection handle for dealing with geographic projections.
 */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "turtle.h"
#include "turtle_projection.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* Routines for map projections. */
static enum turtle_return project_lambert(
	const struct turtle_projection * projection, double latitude,
	double longitude, double * x, double * y);
static enum turtle_return unproject_lambert(
	const struct turtle_projection * projection, double x, double y,
	double * latitude, double * longitude);
static enum turtle_return project_utm(
	const struct turtle_projection * projection, double latitude,
	double longitude, double * x, double * y);
static enum turtle_return unproject_utm(
	const struct turtle_projection * projection, double x, double y,
	double * latitude, double * longitude);

/* Allocate a new projection handle. */
enum turtle_return turtle_projection_create(const char * name,
	struct turtle_projection ** projection)
{
	/* Parse the config from the name string. */
	*projection = NULL;
	struct turtle_projection tmp;
	enum turtle_return rc = turtle_projection_configure(name, &tmp);
	if (rc != TURTLE_RETURN_SUCCESS) return rc;

	/* Allocate memory for the new handle. */
	*projection = malloc(sizeof(**projection));
	if (*projection == NULL) return TURTLE_RETURN_MEMORY_ERROR;

	/* Copy the config and return. */
	memcpy(*projection, &tmp, sizeof(tmp));
	return TURTLE_RETURN_SUCCESS;
}

/* Release the projection memory. */
void turtle_projection_destroy(struct turtle_projection ** projection)
{
	if ((projection == NULL) || (*projection == NULL)) return;
	free(*projection);
	*projection = NULL;
}

/* Helper function to strip a word in a sentence. */
static int locate_word(const char ** str)
{
	const char * p = *str;
	while (*p == ' ') p++;
	*str = p;
	int n = 0;
	while ((*p != ' ') && (*p != '\0')) { p++; n++; }
	return n;
}

/* Configure a projection from a name string. */
enum turtle_return turtle_projection_configure(const char * name,
	struct turtle_projection * projection)
{
	projection->type = PROJECTION_NONE;
	if (name == NULL) return TURTLE_RETURN_SUCCESS;

	/* Locate the 1st word. */
	const char * p = name;
	int n = locate_word(&p);

	if (n == 0) {
		/* No projection has been defined. */
		return TURTLE_RETURN_BAD_PROJECTION;
	}
	else if (strncmp(p, "Lambert", n) == 0) {
		/* This is a Lambert projection. Let's parse the parameters
		 * set.
		 */
		projection->type = PROJECTION_LAMBERT;
		p += n;
		n = locate_word(&p);
		const char * tag[6] = {"I", "II", "IIe", "III", "IV", "93"};
		int i;
		for (i = 0; i < 6; i++) {
			if (strncmp(p, tag[i], n) == 0) {
				projection->settings.lambert_tag = i;
				return TURTLE_RETURN_SUCCESS;
			}

		}
	}
	else if (strncmp(p, "UTM", n) == 0) {
		/* This is a UTM projection. Let's parse the parameters
		 * set.
		 */
		projection->type = PROJECTION_UTM;
		p += n;
		int zone;
		char hemisphere;
		if (sscanf(p, "%d%c", &zone, &hemisphere) != 2)
			return TURTLE_RETURN_BAD_PROJECTION;
		if (hemisphere == '.') {
			double longitude_0;
			if (sscanf(p, "%lf%c", &longitude_0,
				&hemisphere) != 2)
				return TURTLE_RETURN_BAD_PROJECTION;
			projection->settings.utm.longitude_0 = longitude_0;
		}
		else
			projection->settings.utm.longitude_0 = 6.*zone-183.;
		if (hemisphere == 'N')
			projection->settings.utm.hemisphere = 1;
		else if (hemisphere == 'S')
			projection->settings.utm.hemisphere = -1;
		else
			return TURTLE_RETURN_BAD_PROJECTION;
		return TURTLE_RETURN_SUCCESS;
	}

	return TURTLE_RETURN_BAD_PROJECTION;
}

/* Get the name string corresponding to the projection configuration. */
enum turtle_return turtle_projection_info(
	const struct turtle_projection * projection, char ** name)
{
	*name = NULL;
	if ((projection == NULL) || (projection->type == PROJECTION_NONE))
		return TURTLE_RETURN_SUCCESS;

	enum turtle_return rc = TURTLE_RETURN_MEMORY_ERROR;
	int size, nw = 0;
	for (size = 128;; size += 128) {
		char * tmp = realloc(*name, size);
		if (tmp == NULL) goto error;
		*name = tmp;

		if (projection->type == PROJECTION_LAMBERT) {
			const char * tag[6] = {"I", "II", "IIe", "III", "IV",
				"93"};
			nw = snprintf(*name, size, "Lambert %s",
				tag[projection->settings.lambert_tag]);
			if (nw < size) break;
		}
		else if (projection->type == PROJECTION_UTM) {
			char hemisphere =
				(projection->settings.utm.hemisphere > 0) ?
				'N' : 'S';
			double tmp = projection->settings.utm.longitude_0/
				6.+183.;
			int zone = (int)tmp;
			if (fabs(tmp-zone) <= FLT_EPSILON)
				nw = snprintf(*name, size, "UTM %d%c",
					zone, hemisphere);
			else
				nw = snprintf(*name, size, "UTM %.12lg%c",
					projection->settings.utm.longitude_0,
					hemisphere);
			if (nw < size) break;
		}
		else {
			rc = TURTLE_RETURN_BAD_PROJECTION;
			goto error;
		}
	}

	nw++;
	if (nw < size) *name = realloc(*name, nw);
	return TURTLE_RETURN_SUCCESS;

error:
	free(*name);
	*name = NULL;
	return rc;
}

/* Project geodetic coordinates to flat ones. */
enum turtle_return turtle_projection_project(
	const struct turtle_projection * projection, double latitude,
	double longitude, double * x, double * y)
{
	*x = 0.;
	*y = 0.;

	if (projection == NULL)
		return TURTLE_RETURN_BAD_ADDRESS;
	else if (projection->type == PROJECTION_NONE)
		return TURTLE_RETURN_BAD_PROJECTION;
	else if (projection->type == PROJECTION_LAMBERT)
		return project_lambert(projection, latitude, longitude, x, y);
	else
		return project_utm(projection, latitude, longitude, x, y);
}

/* Unproject flat coordinates to geodetic ones. */
enum turtle_return turtle_projection_unproject(
	const struct turtle_projection * projection, double x, double y,
	double * latitude, double * longitude)
{
	*latitude = 0.;
	*longitude = 0.;

	if (projection == NULL)
		return TURTLE_RETURN_BAD_ADDRESS;
	else if (projection->type == PROJECTION_NONE)
		return TURTLE_RETURN_BAD_PROJECTION;
	else if (projection->type == PROJECTION_LAMBERT)
		return unproject_lambert(projection, x, y, latitude, longitude);
	else
		return unproject_utm(projection, x, y, latitude, longitude);
}

/* Compute the isometric latitude for Lambert projections.
 *
 * Source:
 * 	ALG0001 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
static double lambert_latitude_to_iso(double latitude, double e)
{
	const double phi = latitude*M_PI/180.;
	const double s = sin(phi);
	return log(tan(0.25*M_PI+0.5*phi)*pow((1.-e*s)/(1.+e*s), 0.5*e));
}

/* Compute the geographic latitude from the isometric one for Lambert
 * projections.
 *
 * Source:
 * 	ALG0001 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
static double lambert_iso_to_latitude(double L, double e)
{
	const double epsilon = FLT_EPSILON;

	const double eL = exp(L);
	double phi0 = 2.*atan(eL)-0.5*M_PI;
	for (;;) {
		const double s = sin(phi0);
		double phi1 = 2.*atan(pow((1.+e*s)/(1.-e*s), 0.5*e)*eL)
			-0.5*M_PI;
		if (fabs(phi1-phi0) <= epsilon) return phi1/M_PI*180.;
		phi0 = phi1;
	}
}

/* Parameters for a Lambert projection. */
struct lambert_parameters {
	double e;
	double n;
	double c;
	double lambda_c;
	double xs;
	double ys;
};

/* Compute the projected coordinates for Lambert projections.
 *
 * Source:
 * 	ALG0003 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
static void lambert_ll_to_xy(double latitude, double longitude,
	struct lambert_parameters * parameters, double * x, double * y)
{
	const double L = lambert_latitude_to_iso(latitude, parameters->e);
	const double cenL = parameters->c*exp(-parameters->n*L);
	const double lambda = longitude/180.*M_PI;
	const double theta = parameters->n*(lambda-parameters->lambda_c);
	*x = parameters->xs+cenL*sin(theta);
	*y = parameters->ys-cenL*cos(theta);
}

/* Compute the geodetic coordinates from the projected ones for a Lambert
 * projection.
 *
 * Source:
 * 	ALG0004 from http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 */
static void lambert_xy_to_ll(double x, double y,
	struct lambert_parameters * parameters, double * latitude,
	double * longitude)
{
	const double dx = x-parameters->xs;
	const double dy = y-parameters->ys;
	const double R = sqrt(dx*dx+dy*dy);
	const double gamma = atan2(dx, -dy);
	*longitude = (parameters->lambda_c+gamma/parameters->n)*180./M_PI;
	const double L = -log(R/parameters->c)/parameters->n;
	*latitude = lambert_iso_to_latitude(L, parameters->e);
}

/* Get the parameters for a specific Lambert projection.
 * Beware: there is no bound check.
 *
 * Source:
 * 	http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	algorithmes/notice/NTG_71.pdf.
 * 	http://geodesie.ign.fr/contenu/fichiers/documentation/
 * 	rgf93/Lambert-93.pdf (for Lambert 93/RGF93).
 */
static struct lambert_parameters * lambert_get_parameters(int tag)
{
	static struct lambert_parameters parameters[6] = {
		{0.08248325676, 0.7604059656, 11603796.98, 0.04079234433,
			600000.0, 5657616.674},
		{0.08248325676, 0.7289686274, 11745793.39, 0.04079234433,
			600000.0, 6199695.768},
		{0.08248325676, 0.7289686274, 11745793.39, 0.04079234433,
			600000.0, 8199695.768},
		{0.08248325676, 0.6959127966, 11947992.52, 0.04079234433,
			600000.0, 6791905.085},
		{0.08248325676, 0.6712679322, 12136281.99, 0.04079234433,
			234.358, 7239161.542},
		/* Lambert 93/RGF93 have been recomputed from specs. NTG_71
		 * values do not conform to RGF93. */
		{0.08181919112, 0.7253743710, 11755528.70, 0.05235987756,
			700000.0, 12657560.145}
	};

	return parameters+tag;
}

/* Encapsulation of Lambert projections. */
static enum turtle_return project_lambert(
	const struct turtle_projection * projection, double latitude,
	double longitude, double * x, double * y)
{
	struct lambert_parameters * parameters = lambert_get_parameters(
		projection->settings.lambert_tag);
	lambert_ll_to_xy(latitude, longitude, parameters, x, y);
	return TURTLE_RETURN_SUCCESS;
}

/* Encapsulation of inverse Lambert projections.  */
static enum turtle_return unproject_lambert(
	const struct turtle_projection * projection, double x, double y,
	double * latitude, double * longitude)
{
	struct lambert_parameters * parameters = lambert_get_parameters(
		projection->settings.lambert_tag);
	lambert_xy_to_ll(x, y, parameters, latitude, longitude);
	return TURTLE_RETURN_SUCCESS;
}

/* Compute the projected coordinates for UTM projections.
 *
 * Source:
 * 	Wikipedia https://en.wikipedia.org/wiki/
 * 	Universal_Transverse_Mercator_coordinate_system.
 */
static void utm_ll_to_xy(double latitude, double longitude, double longitude_0,
	int hemisphere, double * x, double * y)
{
	const double a = 6378.137E+03;
	const double f = 1./298.257223563;
	const double E0 = 5E+05;
	const double N0 = (hemisphere > 0) ? 0. : 1E+07;
	const double k0 = 0.9996;

	const double n = f/(2.-f);
	const double A = a/(1.+n)*(1.+n*n*(0.25+0.0625*n*n));
	const double alpha[3] = {n*(0.5+n*(-2./3.+5./16.*n)),
		n*n*(13./48.-3./5.*n), 61./240.*n*n*n};

	const double c = 2.*sqrt(n)/(1.+n);
	const double s = sin(latitude*M_PI/180.);
	const double t = sinh(atanh(s)-c*atanh(c*s));
	const double dl = (longitude-longitude_0)*M_PI/180.;
	const double zeta = atan2(t, cos(dl));
	const double eta = atanh(sin(dl)/sqrt(1.+t*t));

	double xs = 0., ys = 0.;
	int i;
	for (i = 0; i < 3; i++) {
		xs += alpha[i]*cos(2.*(i+1)*zeta)*sinh(2.*(i+1)*eta);
		ys += alpha[i]*sin(2.*(i+1)*zeta)*cosh(2.*(i+1)*eta);
	}
	*x = E0+k0*A*(eta+xs);
	*y = N0+k0*A*(zeta+ys);
}

/* Compute the geodetic coordinates from the projected ones for a UTM
 * projection.
 *
 * Source:
 * 	Wikipedia https://en.wikipedia.org/wiki/
 * 	Universal_Transverse_Mercator_coordinate_system.
 */
static void utm_xy_to_ll(double x, double y, double longitude_0, int hemisphere,
	double * latitude, double * longitude)
{
	const double a = 6378.137E+03;
	const double f = 1./298.257223563;
	const double E0 = 5E+05;
	const double N0 = (hemisphere > 0) ? 0. : 1E+07;
	const double k0 = 0.9996;

	const double n = f/(2.-f);
	const double A = a/(1.+n)*(1.+n*n*(0.25+0.0625*n*n));
	const double beta[3] = {n*(0.5+n*(-2./3.+37./96.*n)),
		n*n*(1./48.+1./15.*n), 17./480.*n*n*n
	};
	const double delta[3] = {n*(2.+n*(-2./3.-2.*n)),
		n*n*(7./3.-8./5.*n), 56./15.*n*n*n
	};

	const double zeta0 = (y-N0)/(k0*A);
	const double eta0 = (x-E0)/(k0*A);
	double zeta = zeta0, eta = eta0;
	int i;
	for (i = 0; i < 3; i++) {
		zeta -= beta[i]*sin(2.*(i+1)*zeta0)*cosh(2.*(i+1)*eta0);
		eta -= beta[i]*cos(2.*(i+1)*zeta0)*sinh(2.*(i+1)*eta0);
	}
	const double chi = asin(sin(zeta)/cosh(eta));
	double s = 0.;
	for (i = 0; i < 3; i++) s += delta[i]*sin(2.*(i+1)*chi);
	*latitude = (chi+s)*180./M_PI;
	*longitude = longitude_0+atan2(sinh(eta), cos(zeta))*180./M_PI;
}

/* Encapsulation of UTM projections. */
static enum turtle_return project_utm(
	const struct turtle_projection * projection, double latitude,
	double longitude, double * x, double * y)
{
	utm_ll_to_xy(latitude, longitude, projection->settings.utm.longitude_0,
		projection->settings.utm.hemisphere, x, y);
	return TURTLE_RETURN_SUCCESS;
}

/* Encapsulation of inverse UTM projections.  */
static enum turtle_return unproject_utm(
	const struct turtle_projection * projection, double x, double y,
	double * latitude, double * longitude)
{
	utm_xy_to_ll(x, y, projection->settings.utm.longitude_0,
		projection->settings.utm.hemisphere, latitude, longitude);
	return TURTLE_RETURN_SUCCESS;
}

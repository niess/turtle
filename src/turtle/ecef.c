/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicUles over Long rangEs (TURTLE)
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
 * Turtle utilities for converting from/to ECEF coordinates.
 */
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "turtle.h"

#ifndef M_PI
/* Define pi, if unknown. */
#define M_PI 3.14159265358979323846
#endif

/* WGS84 ellipsoid parameters  */
#define WGS84_A 6378137
#define WGS84_E 0.081819190842622

/* Compute ECEF coordinates from latitude and longitude. */
void turtle_ecef_from_geodetic(
    double latitude, double longitude, double elevation, double ecef[3])
{
        /* Get the parameters of the reference ellipsoid. */
        const double a = WGS84_A, e = WGS84_E;

        /* Compute the Cartesian coordinates. */
        const double s = sin(latitude * M_PI / 180.);
        const double c = cos(latitude * M_PI / 180.);
        const double R = a / sqrt(1. - e * e * s * s);

        ecef[0] = (R + elevation) * c * cos(longitude * M_PI / 180.);
        ecef[1] = (R + elevation) * c * sin(longitude * M_PI / 180.);
        ecef[2] = (R * (1. - e * e) + elevation) * s;
}

/* Compute the geodetic coordinates from the ECEF ones.
 *
 * Reference: B. R. Bowring's 1985 algorithm (single iteration).
 */
void turtle_ecef_to_geodetic(const double ecef[3], double * latitude,
    double * longitude, double * altitude)
{
        /* Get the parameters of the reference ellipsoid. */
        const double a = WGS84_A, e = WGS84_E;
        const double b2 = a * a * (1. - e * e);
        const double b = sqrt(b2);
        const double eb2 = e * e * a * a / b2;

        /* Compute the geodetic coordinates. */
        if ((ecef[0] == 0.) && (ecef[1] == 0.)) {
                if (latitude != NULL) *latitude = (ecef[2] >= 0.) ? 90. : -90.;
                if (longitude != NULL) *longitude = 0.0;
                if (altitude != NULL) *altitude = fabs(ecef[2]) - b;
                return;
        }

        if (longitude != NULL)
                *longitude = atan2(ecef[1], ecef[0]) * 180. / M_PI;
        if ((latitude == NULL) && (altitude == NULL)) return;

        const double p2 = ecef[0] * ecef[0] + ecef[1] * ecef[1];
        const double p = sqrt(p2);
        if (ecef[2] == 0.) {
                if (latitude != NULL) *latitude = 0.;
                if (altitude != NULL) *altitude = p - a;
                return;
        }

        const double r = sqrt(p2 + ecef[2] * ecef[2]);
        const double tu = b * ecef[2] * (1. + eb2 * b / r) / (a * p);
        const double tu2 = tu * tu;
        const double cu = 1. / sqrt(1. + tu2);
        const double su = cu * tu;
        const double tp =
            (ecef[2] + eb2 * b * su * su * su) / (p - e * e * a * cu * cu * cu);
        if (latitude != NULL) *latitude = atan(tp) * 180. / M_PI;

        if (altitude != NULL) {
                const double cp = 1. / sqrt(1.0 + tp * tp);
                const double sp = cp * tp;
                *altitude =
                    p * cp + ecef[2] * sp - a * sqrt(1. - e * e * sp * sp);
        }
}

/* Compute the local East, North, Up (ENU) basis vectors. .
 *
 * Reference: https://en.wikipedia.org/wiki/Horizontal_coordinate_system.
 */
static inline void compute_enu(
    double latitude, double longitude, double e[3], double n[3], double u[3])
{
        const double lambda = longitude * M_PI / 180.;
        const double phi = latitude * M_PI / 180.;
        const double sl = sin(lambda);
        const double cl = cos(lambda);
        const double sp = sin(phi);
        const double cp = cos(phi);
        e[0] = -sl;
        e[1] = cl;
        e[2] = 0.;
        n[0] = -cl * sp;
        n[1] = -sl * sp;
        n[2] = cp;
        u[0] = cl * cp;
        u[1] = sl * cp;
        u[2] = sp;
}

/* Compute the direction vector in ECEF from the horizontal coordinates.
 *
 * Reference: https://en.wikipedia.org/wiki/Horizontal_coordinate_system.
 */
void turtle_ecef_from_horizontal(double latitude, double longitude,
    double azimuth, double elevation, double direction[3])
{
        /* Compute the local E, N, U basis vectors. */
        double e[3], n[3], u[3];
        compute_enu(latitude, longitude, e, n, u);

        /* Project on the E,N,U basis. */
        const double az = azimuth * M_PI / 180.;
        const double el = elevation * M_PI / 180.;
        const double ce = cos(el);
        const double r[3] = { ce * sin(az), ce * cos(az), sin(el) };

        direction[0] = r[0] * e[0] + r[1] * n[0] + r[2] * u[0];
        direction[1] = r[0] * e[1] + r[1] * n[1] + r[2] * u[1];
        direction[2] = r[0] * e[2] + r[1] * n[2] + r[2] * u[2];
}

void turtle_ecef_to_horizontal(double latitude, double longitude,
    const double direction[3], double * azimuth, double * elevation)
{
        /* Compute the local E, N, U basis vectors. */
        double e[3], n[3], u[3];
        compute_enu(latitude, longitude, e, n, u);

        /* Project on the E,N,U basis. */
        const double x =
            e[0] * direction[0] + e[1] * direction[1] + e[2] * direction[2];
        const double y =
            n[0] * direction[0] + n[1] * direction[1] + n[2] * direction[2];
        const double z =
            u[0] * direction[0] + u[1] * direction[1] + u[2] * direction[2];
        double r = direction[0] * direction[0] + direction[1] * direction[1] +
            direction[2] * direction[2];
        if (r <= FLT_EPSILON) return;
        r = sqrt(r);
        if (azimuth != NULL) *azimuth = atan2(x, y) * 180. / M_PI;
        if (elevation != NULL) *elevation = asin(z / r) * 180. / M_PI;
}

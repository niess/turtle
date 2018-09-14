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
 * Turtle utilities for converting from/to ECEF coordinates
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

/* Compute ECEF coordinates from latitude and longitude */
void turtle_ecef_from_geodetic(
    double latitude, double longitude, double elevation, double ecef[3])
{
        /* Get the parameters of the reference ellipsoid */
        const double a = WGS84_A, e = WGS84_E;

        /* Compute the Cartesian coordinates */
        const double s = sin(latitude * M_PI / 180.);
        const double c = cos(latitude * M_PI / 180.);
        const double R = a / sqrt(1. - e * e * s * s);

        ecef[0] = (R + elevation) * c * cos(longitude * M_PI / 180.);
        ecef[1] = (R + elevation) * c * sin(longitude * M_PI / 180.);
        ecef[2] = (R * (1. - e * e) + elevation) * s;
}

/* Compute the geodetic coordinates from the ECEF ones
 *
 * Reference: Olson, D. K. (1996). "Converting earth-Centered,
 * Earth-Fixed Coordinates to Geodetic Coordinates," IEEE Transactions on
 * Aerospace and Electronic Systems, Vol. 32, No. 1, January 1996, pp. 473-476
 */
void turtle_ecef_to_geodetic(const double ecef[3], double * latitude,
    double * longitude, double * altitude)
{
        /* Get the parameters of the reference ellipsoid */
        const double a = WGS84_A;
        const double e2 = WGS84_E * WGS84_E;
        const double a1 = a * e2;
        const double a2 = a1 * a1;
        const double a3 = 0.5 * a1 * e2;
        const double a4 = 2.5 * a2;
        const double a5 = a1 + a3;
        const double a6 = 1. - e2;
        
        /* Check special cases */
        if ((ecef[0] == 0.) && (ecef[1] == 0.)) {
                if (latitude != NULL) *latitude = (ecef[2] >= 0.) ? 90. : -90.;
                if (longitude != NULL) *longitude = 0.0;
                if (altitude != NULL) *altitude = fabs(ecef[2]) - sqrt(a2 * a6);
                return;
        }

        if (longitude != NULL)
                *longitude = atan2(ecef[1], ecef[0]) * 180. / M_PI;
        if ((latitude == NULL) && (altitude == NULL)) return;
        
        /* Compute the geodetic coordinates */
        const double zp = fabs(ecef[2]);
        const double w2 = ecef[0] * ecef[0] + ecef[1] * ecef[1];
        const double w = sqrt(w2);
        const double z2 = ecef[2] * ecef[2];
        const double r2 = w2 + z2;
        const double r = sqrt(r2);
        const double s2 = z2 / r2;
        const double c2 = w2 / r2;
        
        double c, s, ss, la;
        if (c2 > 0.3) {
                const double u = a2 / r;
                const double v = a3 - a4 / r;
                s = (zp / r) * (1. + c2 * (a1 + u + s2 * v) / r);
                la = asin(s);
                ss = s * s;
                c = sqrt(1. - ss);
        } else {
                const double u = a2 / r;
                const double v = a3 - a4 / r;
                c = (w / r) * (1. - s2 * (a5 - u - c2 * v) / r);
                la = acos(c);
                ss = 1. - c * c;
                s = sqrt(ss);
        }
        
        const double g = 1. - e2 * ss;
        const double rg = a / sqrt(g);
        const double rf = a6 * rg;
        const double u = w - rg * c;
        const double v = zp - rf * s;
        const double f = c * u + s * v;
        const double m = c * v - s * u;
        const double p = m / (rf / g + f);
        
        la += p;
        if (ecef[2] < 0.) la = -la;
        if (latitude != NULL) *latitude = la * 180. / M_PI;
        if (altitude != NULL) *altitude = f + 0.5 * m * p;
}

/* Compute the local East, North, Up (ENU) basis vectors
 *
 * Reference: https://en.wikipedia.org/wiki/Horizontal_coordinate_system
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

/* Compute the direction vector in ECEF from the horizontal coordinates
 *
 * Reference: https://en.wikipedia.org/wiki/Horizontal_coordinate_system
 */
void turtle_ecef_from_horizontal(double latitude, double longitude,
    double azimuth, double elevation, double direction[3])
{
        /* Compute the local E, N, U basis vectors */
        double e[3], n[3], u[3];
        compute_enu(latitude, longitude, e, n, u);

        /* Project on the E,N,U basis */
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
        /* Compute the local E, N, U basis vectors */
        double e[3], n[3], u[3];
        compute_enu(latitude, longitude, e, n, u);

        /* Project on the E,N,U basis */
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

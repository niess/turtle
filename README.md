# TURTLE
( **T**opographic **U**tilities for **R**endering **T**he e**LE**vation )

## Description

TURTLE is a C99 library for rendering a terrain elevation from various
**D**igital **E**levation **M**odels (**DEM**s). Its main features are:

* Support for local projection maps (**UTM**, **Lambert**, **RGF93**) as well
as for world wide models with tiles, e.g. [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp).

* Provide utilities for frame transforms between the supported projections,
geodetic coordinates and Cartesian **E**arth-**C**entered, **E**arth-**F**ixed
(**ECEF**) coordinates.

* Provide a thread safe interface for accessing world wide **DEM**s,
optimised for tracking problems. It allows to balance execution speed, I/Os and
memory usage.

## Installation

Currently there is no automatic build procedure. If on a Linux box you might
try and adapt the provided `setup.sh` and `Makefile`. The source code conforms
to C99 however for loading or dumping **DEM**s, you will need the following
librairies:

* **libpng** for local maps.
* **libtiff** for geotiff data, e.g. [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp) or
[GEBCO](http://www.gebco.net/).

Note that build options allow to disable either or both formats and their
dependencies if not needed.

## Documentation

The API documentation can be found
[here](https://niess.github.io/turtle/docs/index.html#HEAD), as well as some
basic examples of usage.

## License

The TURTLE library is  under the **GNU LGPLv3** license. See the provided
`LICENSE` and `COPYING.LESSER` files.

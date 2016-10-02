# TURTLE
( **T**opographic **U**tilities for **R**endering **T**he e**LE**vation )

## Description

TURTLE is a C99 library for rendering a terrain elevation from various
**D**igital **E**levation **M**odels (**DEM**s). Its main features are:

* Support for local projection maps (**UTM**, **Lambert**, **RGF93**) as well
as for the [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp) world wide
model.

* Provide utilities for frame transforms between the supported projections,
geodetic coordinates and Cartesian **E**arth-**C**entered, **E**arth-**F**ixed (**ECEF**) coordinates.

* Provide a thread safe interface for accessing world wide **DEM**s,
optimised for tracking problems. It allows to balance execution speed, I/Os and
memory usage.

## Installation

Currently there is no automatic build procedure. If on a Linux box you might
try and adapt the provided `setup.sh` and `Makefile`. The source code conforms
to C99 however for loading or dumping **DEM**s, you will need the following librariries:
**libpng** for local maps and **libtiff** for ASTER-GDEM data. Note that build options
allow to disable either or both formats and their dependencies if not needed.

## Documentation

The API documentation can be found [here](https://niess.github.io/turtle/docs/index.html#HEAD), as well as some basic examples of usage.

## License
The TURTLE library is  under the **GPL-3.0** license. See the provided `LICENSE` file.

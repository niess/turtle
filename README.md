# TURTLE
( **T**opographic **U**tilities for **R**endering **T**he e**LE**vation )

## Description

TURTLE is a C99 library for rendering a terrain elevation from various
**D**igital **E**levation **M**odels (**DEM**s). Its main features are:

* Support for local projection maps (**UTM**, **Lambert**, **RGF93**) as well
as for world wide models with tiles, e.g. [SRTMGL1](https://lpdaac.usgs.gov/node/527)
or [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp).

* Provide utilities for frame transforms between the supported projections,
geodetic coordinates and Cartesian **E**arth-**C**entered, **E**arth-**F**ixed
(**ECEF**) coordinates.

* Provide a thread safe interface for accessing world wide **DEM**s,
optimised for tracking problems. It allows to balance execution speed, I/Os and
memory usage.

## Installation

Currently there is no automatic build procedure. If on a Linux box you might
try and adapt the provided `setup.sh` and `Makefile`. The source code conforms
to C99. For loading or dumping **DEM**s, you will need the following
libraries and packages:

* **libpng** and [JSMN][JSMN] for local maps.
* **libtiff** for geotiff data, e.g. [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp)
  or [GEBCO](http://www.gebco.net/).
* [TinyDir][TinyDir] for OS independent directories management.

Note that build options allow to disable either or both of PNG or TIFF formats,
and their dependencies, if not needed.

Note also that while **libpng** and **libtiff** are expected to be installed
on your system, you can directly get [JSMN][JSMN] and [TinyDir][TinyDir] as
submodules, e.g. using `git clone --recursive`.

[JSMN]: https://github.com/zserge/jsmn
[TinyDIr]: https://github.com/cxong/tinydir

## Documentation

The API documentation can be found [here](http://niess.github.io/turtle-docs).
You might directly check the [examples](examples) as well.

## License

The TURTLE library is  under the **GNU LGPLv3** license. See the provided
`LICENSE` and `COPYING.LESSER` files.

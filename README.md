# TURTLE
( **T**opographic **U**tilities for t**R**acking **T**he e**LE**vation )

## Description

TURTLE is a C99 library for rendering a terrain elevation from various
**D**igital **E**levation **M**odels (**DEM**s). It is optimised for tracking
problems, e.g. Monte-Carlo transport of particles, where only a subset of the
terrain is sampled, on the fly. Its main features are:

* Support for local projection maps (**UTM**, **Lambert**, **RGF93**) as well
as for tiled world wide models, e.g. [SRTMGL1](https://lpdaac.usgs.gov/node/527)
or [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp).

* Provide utilities for frame transforms between the supported projections,
geodetic coordinates and Cartesian **E**arth-**C**entered, **E**arth-**F**ixed
(**ECEF**) coordinates.

* Provide a thread safe interface for accessing **DEM**s from multiple sources,
optimised for tracking problems. It allows to balance execution speed, I/Os and
memory usage.

Note that TURTLE is not an ~image library~. It can only load a few commonly
used data formats for geographic maps, i.e: **GEOTIFF**, **GRD** and **HGT**.
Binary data formats must be 16b and grayscale. In addition, maps can be loaded
and dumped in **PNG**, enriched with a custom header (as a `tEXt` chunk).

## Installation

Currently there is no automatic build procedure. If on a Linux box you might
try and adapt the provided `Makefile`. The source code conforms to C99. For
loading or dumping **DEM**s, you will need the following libraries and packages:

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
`LICENSE` and `COPYING.LESSER` files. The [examples](examples) however have a
separate public domain license allowing them to be copied without restrictions.

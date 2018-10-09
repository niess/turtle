[![Build Status](https://travis-ci.com/niess/turtle.svg?branch=master)](https://travis-ci.com/niess/turtle)
[![codecov](https://codecov.io/gh/niess/turtle/branch/master/graph/badge.svg)](https://codecov.io/gh/niess/turtle)

# TURTLE
( **T**opographic **U**tilities for t**R**ansporting par**T**icules over **L**ong rang**E**s )

## Description

TURTLE is a C99 library providing utilities for the long range transport of
particles through a topography, described by **D**igital **E**levation
**M**odels (**DEM**s). Its main features are:

* Support for local projection maps (**UTM**, **Lambert**, **RGF93**) as well
as for tiled world wide models, e.g. [SRTMGL1](https://lpdaac.usgs.gov/node/527)
or [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp).

* Provide utilities for frame transforms between the supported projections,
geodetic coordinates and Cartesian **E**arth-**C**entered, **E**arth-**F**ixed
(**ECEF**) coordinates.

* Provide a thread safe interface for accessing **DEM**s from multiple sources,
optimised for transport problems. It allows to balance execution speed, I/Os and
memory usage.

Note that TURTLE is not an ~image library~ neither a ~~Monte-Carlo transport
engine~~. It can only load a few commonly used data formats for geographic
maps, i.e: **GEOTIFF**, **GRD** and **HGT**. Binary data formats must be 16b
and grayscale. In addition, maps can be loaded and dumped in **PNG**, enriched
with a custom header (as a `tEXt` chunk).

## Installation

The TURTLE library should be straightforward to compile on Linux and OSX. It
wasn't tested on Windows however. An example of [`Makefile`](Makefile) is
shipped with the sources. It allows to build TURTLE (as a shared library:
`libturtle.so`) and the examples, e.g. as:
```bash
make && make examples
```

The TURTLE source code conforms to C99 and has little dependencies except on
the C89 standard library. Note however that for loading or dumping **DEM**s,
you might also need the following external libraries:

* **libpng** for custom TURTLE dumps.
* **libtiff** for loading GEOTIFF data, e.g.
  [ASTER-GDEM2](https://asterweb.jpl.nasa.gov/gdem.asp) or
  [GEBCO](http://www.gebco.net/).

Those are rather standard though and might already be installed on your system.
In addition, build options in the Makefile allow to disable either or both of
PNG or TIFF formats, and their dependencies, if not needed.

## Documentation

The API documentation can be found [here](http://niess.github.io/turtle-docs).
You might directly check the [examples](examples) as well.

## License

The TURTLE library is  under the **GNU LGPLv3** license. See the provided
[`LICENSE`](LICENSE) and [`COPYING.LESSER`](COPYING.LESSER) files. The
[examples](examples) however have a separate public domain license allowing
them to be copied without restrictions.

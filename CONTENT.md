- # [CONTENT.md](CONTENT.md)
  The present file, describing the content of this package.

- # [COPYING.LESSER](COPYING.LESSER)
  The specific LGPL-3.0 licensing data.

- # [examples/](examples)
  Examples of usage of the TURTLE library.
  
  - ## [examples/example-demo.c](examples/example-demo.c)
    Example illustrating the API of the TURTLE library. Note that you'll
    need to run the `projection` example first, in order to generate a local
    map.
  
  - ## [examples/example-projection.c](examples/example-projection.c)
    Example of usage of a TURTLE projection object. Note that external
     data tiles are required, E.g. from SRTMGL1.
  
  - ## [examples/example-pthread.c](examples/example-pthread.c)
    Example of usage of a TURTLE client object for multi-threaded applications.
     Note that external data tiles are required as well.
  
  - ## [examples/example-stepper.c](examples/example-stepper.c)
    Example of usage of a TURTLE stepper object for ray tracing or Monte-carlo
    stepping. Note that several external data are required as detailed in the
    preamble of the file.
  
  - ## [LICENSE](examples/LICENSE)
    The specific licensing data for the examples (Public domain).

- # [include/turtle.h](include/turtle.h)
  C header file describing the API of the TURTLE library.

- # [LICENSE](LICENSE)
  The generic GPL-3.0 licensing data.

- # [Makefile](Makefile)
  Example of `Makefile` for compiling the TURTLE library and the examples on
  UNIX systems.

- # [README.md](README.md)
  A brief description of the TURTLE library with some guidance for usage.

- # [src/](src)
  The C source for building TURTLE library.

  - ## [src/deps/](src/deps)
    External dependencies packaged with the TURTLE library and re-distributed
    with their own licenses.

    - ### [src/deps/jsmn.c](src/deps/jsmn.c)
    Source of the JSMN library, allowing to parse JSON files. This library is
    distributed under the MIT license.

    - ### [src/deps/jsmn.h](src/deps/jsmn.h)
    Header file of the JSMN library, including the licensing data.

    - ### [src/deps/tinydir.c](src/deps/tinydir.c)
    Source of the TinyDir library, an OS-independent wrapper for browsing
    directories content. This library is distributed under a simplified BSD
    license.

    - ### [src/deps/tinydir.h](src/deps/tinydir.h)
    Header file of the TinyDir library, including the licensing data.

  - ## [src/turtle/](src/turtle)
    The source of the TURTLE library, written in C99.

    - ### [src/turtle/client.c](src/turtle/client.c)
      Implementation of the TURTLE client object. The client object allows
      thread safe access to a stack of maps, E.g. for global DEMs.

    - ### [src/turtle/client.h](src/turtle/client.h)
      Internal definitions for the TURTLE client object.

    - ### [src/turtle/ecef.c](src/turtle/ecef.c)
      Implementation of ECEF conversion routines.

    - ### [src/turtle/error.c](src/turtle/error.c)
      Implementation of error handling routines.

    - ### [src/turtle/error.h](src/turtle/error.h)
      Internal definitions and helper macros for error handling.

    - ### [src/turtle/io/](src/turtle/io)

      - #### [src/turtle/io/geotiff16.c](src/turtle/io/geotiff16.c)
        Implementation of a specialised GEOTIFF reader and writter, restricted
        to 16b grayscale TIFF images.

      - #### [src/turtle/io/grd.c](src/turtle/io/grd.c)
        Implementation of the GRD reader and writter. This format is used
        by the EGM96 geoid grid.

      - #### [src/turtle/io/hgt.c](src/turtle/io/hgt.c)
        Implementation of the HGT reader and writter. This is a raw binary
        format with network byte order, used for example by the SRTMGL1
        tiles.

      - #### [src/turtle/io/png16.c](src/turtle/io/png16.c)
        Implementation of a specialised PNG reader and writter, restricted
        to 16b grayscale images and including meta data in JSON.

    - ### [src/turtle/io.c](src/turtle/io.c)
      Implementation of generic reader and writter objects.

    - ### [src/turtle/io.h](src/turtle/io.h)
      Internal definitions of the reader and writter objects.

    - ### [src/turtle/list.c](src/turtle/list.c)
      Implementation of a chained list used internaly by TURTLE.

    - ### [src/turtle/list.h](src/turtle/list.h)
      Internal definitions for chained lists.

    - ### [src/turtle/map.c](src/turtle/map.c)
      Implementation of the TURTLE map object. This object is a wrapper to
      grid topography data.

    - ### [src/turtle/map.h](src/turtle/map.h)
      Internal definitions for the TURTLE map object.

    - ### [src/turtle/projection.c](src/turtle/projection.c)
      Implementation of the TURTLE projection object. This object allows to
      convert cartographic coordinates to/from geodetic ones.

    - ### [src/turtle/projection.h](src/turtle/projection.h)
      Internal definitions for the TURTLE projection object.

    - ### [src/turtle/stack.c](src/turtle/stack.c)
      Implementation of the TURTLE stack object. This object handles a stack
      of maps, E.g. for global DEM.

    - ### [src/turtle/stack.h](src/turtle/stack.h)
      Internal definitions for the TURTLE stack object.

    - ### [src/turtle/stepper.c](src/turtle/stepper.c)
      Implementation of the TURTLE stepper object. This object is a utility
      for stepping through a topography described by one or more DEMs, E.g.
      in a Monte-Carlo.

    - ### [src/turtle/stepper.h](src/turtle/stepper.h)
      Internal definitions for the TURTLE stepper object.

- # [tests/test-turtle.c](tests/test-turtle.c)
  Unit tests for the TURTLE library. This requires
  [`libcheck`](https://github.com/libcheck/check) which can be installed as:
  `make libcheck`. Then to run the tests do: `make test`.

cmake_minimum_required (VERSION 3.0.0)
project (TURTLE
    LANGUAGES C
    VERSION 0.9
)


# Build options
if (NOT CMAKE_BUILD_TYPE)
        set (CMAKE_BUILD_TYPE "Release" CACHE STRING
                 "Choose the type of build, options are: Debug Release
                  RelWithDebInfo MinSizeRel and Test." FORCE)
endif ()

if (NOT BUILD_SHARED_LIBS)
    set (BUILD_SHARED_LIBS TRUE CACHE BOOL "Build the library as shared")
endif ()

option (TURTLE_USE_GEOTIFF "Enable loading GeoTIFF files" ON)
option (TURTLE_USE_GRD "Enable loading GRD files" ON)
option (TURTLE_USE_HGT "Enable loading HGT files" ON)
option (TURTLE_USE_PNG "Enable dumping and loadind PNG files" ON)
option (TURTLE_USE_ASC "Enable dumping and loadind ASC files" ON)
option (TURTLE_USE_LD "Enable loading PNG and TIFF libraries on the fly" ON)


# Build and install rules for the TURTLE library
add_library (turtle
    include/turtle.h
    src/turtle/client.c src/turtle/client.h
    src/turtle/ecef.c
    src/turtle/error.c src/turtle/error.h
    src/turtle/io.c src/turtle/io.h
    src/turtle/list.c src/turtle/list.h
    src/turtle/map.c src/turtle/map.h
    src/turtle/projection.c src/turtle/projection.h
    src/turtle/stack.c src/turtle/stack.h
    src/turtle/stepper.c src/turtle/stepper.h
    src/deps/tinydir.c src/deps/tinydir.h
)
set_target_properties (turtle PROPERTIES VERSION ${TURTLE_VERSION})
target_include_directories (turtle PRIVATE include src)
target_link_libraries (turtle m)

if (${TURTLE_USE_GEOTIFF})
    target_sources (turtle PRIVATE src/turtle/io/geotiff16.c)
    if (NOT ${TURTLE_USE_LD})
        target_link_libraries (turtle tiff)
    endif ()
else ()
    target_compile_definitions (turtle PRIVATE -DTURTLE_NO_TIFF)
endif ()

if (${TURTLE_USE_GRD})
    target_sources (turtle PRIVATE src/turtle/io/grd.c)
else ()
    target_compile_definitions (turtle PRIVATE -DTURTLE_NO_GRD)
endif ()

if (${TURTLE_USE_HGT})
    target_sources (turtle PRIVATE src/turtle/io/hgt.c)
else ()
    target_compile_definitions (turtle PRIVATE -DTURTLE_NO_HGT)
endif ()

if (${TURTLE_USE_PNG})
    target_sources (turtle PRIVATE
        src/turtle/io/png16.c src/deps/jsmn.c src/deps/jsmn.h
    )
    if (NOT ${TURTLE_USE_LD})
        target_link_libraries (turtle png)
    endif ()
else ()
    target_compile_definitions (turtle PRIVATE -DTURTLE_NO_PNG)
endif ()

if (${TURTLE_USE_ASC})
    target_sources (turtle PRIVATE src/turtle/io/asc.c)
else ()
    target_compile_definitions (turtle PRIVATE -DTURTLE_NO_ASC)
endif ()

if (${TURTLE_USE_LD})
    target_link_libraries (turtle dl)
else ()
    target_compile_definitions (turtle PRIVATE -DTURTLE_NO_LD)
endif ()

install (TARGETS turtle DESTINATION lib)
install (FILES include/turtle.h DESTINATION include)


# Build and install rules for the examples
add_executable (example-demo EXCLUDE_FROM_ALL examples/example-demo.c)
target_link_libraries (example-demo turtle)
target_include_directories (example-demo PRIVATE include)

add_executable (example-projection EXCLUDE_FROM_ALL examples/example-projection.c)
target_link_libraries (example-projection turtle)
target_include_directories (example-projection PRIVATE include)

add_executable (example-pthread EXCLUDE_FROM_ALL examples/example-pthread.c)
target_link_libraries (example-pthread pthread turtle)
target_include_directories (example-pthread PRIVATE include)

add_executable (example-stepper EXCLUDE_FROM_ALL examples/example-stepper.c)
target_link_libraries (example-stepper turtle)
target_include_directories (example-stepper PRIVATE include)

add_custom_target(examples
    DEPENDS example-demo example-projection example-pthread example-stepper
)

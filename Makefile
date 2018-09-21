# Default compilation flags
CFLAGS := -O3 -std=c99 -pedantic -Wall -fPIC
LIBS := -lm
INCLUDES := -Iinclude -Isrc

OBJS := build/client.o build/ecef.o build/error.o build/io.o build/map.o       \
	build/projection.o build/stack.o build/stepper.o build/tinydir.o

# Flag for GEOTIFF files
TURTLE_USE_TIFF := 1
ifeq ($(TURTLE_USE_TIFF), 1)
	LIBS += -ltiff
	OBJS += build/geotiff16.o
else
	CFLAGS += -DTURTLE_NO_TIFF
endif

# Flag for GRD files
TURTLE_USE_GRD := 1
ifeq ($(TURTLE_USE_GRD), 1)
	OBJS += build/grd.o
else
	CFLAGS += -DTURTLE_NO_GRD
endif

# Flag for HGT files
TURTLE_USE_HGT := 1
ifeq ($(TURTLE_USE_HGT), 1)
	OBJS += build/hgt.o
else
	CFLAGS += -DTURTLE_NO_HGT
endif

# Flag for PNG files
TURTLE_USE_PNG := 1
ifeq ($(TURTLE_USE_PNG), 1)
	PACKAGE := libpng
	CFLAGS += $(shell pkg-config --cflags $(PACKAGE))
	LIBS += $(shell pkg-config --libs $(PACKAGE))
	OBJS += build/jsmn.o build/png16.o
else
	CFLAGS += -DTURTLE_NO_PNG
endif

# Available builds
.PHONY: lib clean examples test

# Rules for building the library
lib: lib/libturtle.so

lib/libturtle.so: $(OBJS)
	@mkdir -p lib
	@gcc -o $@ $(CFLAGS) -shared $(INCLUDES) $(OBJS) $(LIBS)

build/%.o: src/turtle/%.c src/turtle/%.h
	@mkdir -p build
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

build/%.o: src/turtle/%.c
	@mkdir -p build
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

build/%.o: src/turtle/io/%.c
	@mkdir -p build
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

build/%.o: src/%.c include/%.h
	@mkdir -p build
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

build/%.o: src/deps/%.c src/deps/%.h
	@mkdir -p build
	@gcc $(CFLAGS) -o $@ -c $<

# Rules for building the examples
examples: bin/example-demo bin/example-projection bin/example-pthread          \
          bin/example-stepper

bin/example-pthread: examples/example-pthread.c lib/libturtle.so
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -Iinclude $< -Llib -Wl,-rpath $(PWD)/lib -lturtle \
             -lpthread

bin/example-%: examples/example-%.c lib/libturtle.so
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -Iinclude $< -Llib -Wl,-rpath $(PWD)/lib -lturtle

# Rules for building the tests binaries
SOURCES := src/turtle/client.c src/turtle/ecef.c src/turtle/error.c            \
	src/turtle/io.c src/turtle/map.c src/turtle/projection.c               \
	src/turtle/stack.c src/turtle/stepper.c src/turtle/io/geotiff16.c      \
	src/turtle/io/grd.c src/turtle/io/hgt.c src/turtle/io/png16.c

test: bin/test-turtle
	@mkdir -p tests/topography
	@./bin/test-turtle
	@rm -rf tests/*.png tests/*.grd tests/*.hgt tests/*.tif                \
		tests/topography/*
	@mv *.gcda tests/.
	@gcov -o tests $(SOURCES)
	@rm -rf tinydir.h.gcov tests/test-turtle.gcno tests/test-turtle.gcda
	@mv *.gcov tests/.

bin/test-%: tests/test-%.c build/jsmn.o build/tinydir.o $(SOURCES)
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -O0 -g -ftest-coverage -fprofile-arcs $(INCLUDES) \
		$^ $(LIBS)
	@mv *.gcno tests/.

# Clean-up rule
clean:
	@rm -rf bin lib build tests/*.gcno tests/*.gcda tests/*.gcov *.gcov    \
		*.gcno *.gcda tests/*.png tests/*.grd tests/*.hgt tests/*.tif  \
		tests/topography

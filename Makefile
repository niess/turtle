# Default compilation flags
CFLAGS := -O3 -std=c99 -pedantic -Wall -fPIC
LIBS := -lm
INCLUDES := -Iinclude -Isrc

OBJS := build/client.o build/ecef.o build/error.o build/io.o build/map.o       \
	build/projection.o build/stack.o build/stepper.o build/turtle.o

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
.PHONY: lib clean examples

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

bin/example-pthread: examples/example-pthread.c
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -Iinclude $< -Llib -Wl,-rpath $(PWD)/lib -lturtle \
             -lpthread

bin/example-%: examples/example-%.c
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -Iinclude $< -Llib -Wl,-rpath $(PWD)/lib -lturtle

# Clean-up rule
clean:
	@rm -rf bin lib build

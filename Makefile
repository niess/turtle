# Default compilation flags
DEPS_DIR := deps

CFLAGS := -O0 -g -std=c99 -pedantic -Wall -fPIC -Wfatal-errors
LIBS := -lm
OBJS := client.o ecef.o error.o map.o projection.o reader.o stack.o stepper.o  \
	turtle.o
INCLUDES := -Iinclude -Isrc -I$(DEPS_DIR)/tinydir

# Flag for PNG files
TURTLE_USE_PNG := 1
ifeq ($(TURTLE_USE_PNG),1)
	PACKAGE := libpng
	CFLAGS += $(shell pkg-config --cflags $(PACKAGE))
	LIBS += $(shell pkg-config --libs $(PACKAGE))
	OBJS += jsmn.o png16.o
	INCLUDES += -I$(DEPS_DIR)/jsmn
else
	CFLAGS += -DTURTLE_NO_PNG
endif

# Flag for GEOTIFF files
TURTLE_USE_TIFF := 1
ifeq ($(TURTLE_USE_TIFF),1)
	LIBS += -ltiff
	OBJS += geotiff16.o
else
	CFLAGS += -DTURTLE_NO_TIFF
endif

# Flag for HGT files
TURTLE_USE_HGT := 1
ifeq ($(TURTLE_USE_HGT),1)
	OBJS += hgt.o
else
	CFLAGS += -DTURTLE_NO_HGT
endif

# Available builds.
.PHONY: lib clean examples

# Rules for building the library.
lib: lib/libturtle.so
	@rm -f *.o

lib/libturtle.so: $(OBJS)
	@mkdir -p lib
	@gcc -o $@ $(CFLAGS) -shared $(INCLUDES) $(OBJS) $(LIBS)

%.o: src/turtle/%.c src/turtle/%.h
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

%.o: src/turtle/%.c
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

%.o: src/turtle/loader/%.c src/turtle/loader/%.h
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

%.o: src/turtle/reader/%.c
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

%.o: src/%.c include/%.h
	@gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

%.o: $(DEPS_DIR)/jsmn/%.c $(DEPS_DIR)/jsmn/%.h
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
	@rm -rf bin lib *.o

# Default compilation flags.
DEPS_DIR := deps

CFLAGS := -O2 -std=c99 -pedantic -Wall -fPIC
LIBS := -lm
OBJS := client.o datum.o error.o loader.o map.o projection.o turtle.o
INC := -Iinclude -I$(DEPS_DIR)/tinydir

# Flag for PNG files.
TURTLE_USE_PNG := 1
ifeq ($(TURTLE_USE_PNG),1)
	PACKAGE := libpng
	CFLAGS += $(shell pkg-config --cflags $(PACKAGE))
	LIBS += $(shell pkg-config --libs $(PACKAGE))
	OBJS +=  jsmn.o
	INC += -I$(DEPS_DIR)/jsmn
else
	CFLAGS += -DTURTLE_NO_PNG
endif

# Flag for GEOTIFF files.
TURTLE_USE_TIFF := 1
ifeq ($(TURTLE_USE_TIFF),1)
	LIBS += -ltiff
	OBJS +=  geotiff16.o
else
	CFLAGS += -DTURTLE_NO_TIFF
endif

# Flag for HGT files.
TURTLE_USE_HGT := 1
ifeq ($(TURTLE_USE_HGT),1)
	OBJS +=  hgt.o
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
	@gcc -o $@ $(CFLAGS) -shared $(INC) $(OBJS) $(LIBS)

%.o: src/turtle/%.c src/turtle/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

%.o: src/turtle/loader/%.c src/turtle/loader/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

%.o: src/%.c include/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

%.o: $(DEPS_DIR)/jsmn/%.c $(DEPS_DIR)/jsmn/%.h
	@gcc $(CFLAGS) -o $@ -c $<

# Rules for building the examples.
examples: bin/example-demo bin/example-projection bin/example-pthread

bin/example-pthread: examples/example-pthread.c
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -Wl,-rpath $(PWD)/lib -lturtle -lpthread

bin/example-%: examples/example-%.c
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -Wl,-rpath $(PWD)/lib -lturtle

# Clean-up rule.
clean:
	@rm -rf bin lib *.o

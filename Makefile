# Default compilation flags.
CFLAGS := -O2 -std=c99 -pedantic -Wall -fPIC
LIBS := -lm
OBJS := turtle.o turtle_projection.o turtle_map.o turtle_datum.o turtle_client.o
INC := -Iinclude -Ideps/tinydir

# Flags for .png files.
USE_PNG := 1
ifeq ($(USE_PNG),1)
	PACKAGE := libpng
	CFLAGS += $(shell pkg-config --cflags $(PACKAGE))
	LIBS += $(shell pkg-config --libs $(PACKAGE))
	OBJS +=  jsmn.o
	INC += -Ideps/jsmn
else
	CFLAGS += -DTURTLE_NO_PNG
endif

# Flags for GEOTIFF files.
USE_TIFF := 1
ifeq ($(USE_TIFF),1)
	LIBS += -ltiff
	OBJS +=  geotiff16.o
else
	CFLAGS += -DTURTLE_NO_TIFF
endif

# Available builds.
.PHONY: lib clean examples

# Rules for building the library.
lib: lib/libturtle.so
	@rm -f *.o

lib/libturtle.so: $(OBJS)
	@mkdir -p lib
	@gcc -o $@ $(CFLAGS) -shared $(INC) $(OBJS) $(LIBS)

%.o: src/%.c src/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

%.o: src/%.c include/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

%.o: deps/jsmn/%.c deps/jsmn/%.h
	@gcc $(CFLAGS) -o $@ -c $<

# Rules for building the examples.
examples: bin/example-demo bin/example-projection bin/example-pthread

bin/example-pthread: examples/example-pthread.c
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle -lpthread

bin/example-%: examples/example-%.c
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle

# Clean-up rule.
clean:
	@rm -rf bin lib *.o

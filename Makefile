# Default compilation flags.
CFLAGS := -O2 -std=c99 -pedantic -Wall -fPIC
LIBS := -lm
OBJS := turtle.o turtle_projection.o turtle_map.o turtle_datum.o turtle_client.o
INC := -Iinclude

# Flags for .png files.
USE_PNG := 1
ifeq ($(USE_PNG),1)
	PACKAGE := libpng12
	CFLAGS += $(shell pkg-config --cflags $(PACKAGE))
	LIBS += $(shell pkg-config --libs $(PACKAGE))
	OBJS +=  jsmn.o
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

# Rules for building the examples.
examples: example-demo example-projection example-pthread

example-pthread: examples/example-pthread.c
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle -lpthread

example-%: examples/example-%.c
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle

# Clean-up rule.
clean:
	@rm -rf example-* lib *.o

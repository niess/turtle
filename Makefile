# Default compilation flags
CFLAGS   = -O3 -std=c99 -Wall
LIBS     = -lm
INCLUDES = -Iinclude -Isrc

OBJS  = build/client.o build/ecef.o build/error.o build/io.o build/list.o      \
	build/map.o build/projection.o build/stack.o build/stepper.o           \
	build/tinydir.o

SOEXT = so
SYS   = $(shell uname -s)
ifeq ($(SYS), Darwin)
	SOEXT = dylib
endif

# Flag for runtime linking of libraries
TURTLE_USE_LD := 1
ifeq ($(TURTLE_USE_LD), 1)
	LIBS += -ldl
else
	CFLAGS += -DTURTLE_NO_LD
endif

# Flag for GEOTIFF files
TURTLE_USE_TIFF := 1
ifeq ($(TURTLE_USE_TIFF), 1)
ifneq ($(TURTLE_USE_LD), 1)
	LIBS += -ltiff
endif
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
ifneq ($(TURTLE_USE_LD), 1)
	LIBS += -lpng16
endif
	OBJS += build/jsmn.o build/png16.o
else
	CFLAGS += -DTURTLE_NO_PNG
endif

# Flag for ASC files
TURTLE_USE_ASC := 1
ifeq ($(TURTLE_USE_ASC), 1)
	OBJS += build/asc.o
else
	CFLAGS += -DTURTLE_NO_ASC
endif


# Available builds
.PHONY: lib clean libcheck examples test


# Rules for building the library
lib: lib/libturtle.$(SOEXT)

SHARED = -shared
RPATH  = '-Wl,-rpath,$$ORIGIN/../lib'
ifeq ($(SYS), Darwin)
	SHARED = -dynamiclib -Wl,-install_name,@rpath/libturtle.$(SOEXT)
	RPATH  = -Wl,-rpath,@loader_path/../lib
endif

lib/libturtle.$(SOEXT): $(OBJS) include/turtle.h
	@mkdir -p lib
	@gcc -o $@ $(LDFLAGS) $(SHARED) $(INCLUDES) $(OBJS) $(LIBS)

build/%.o: src/turtle/%.c src/turtle/%.h
	@mkdir -p build
	@gcc $(CFLAGS) -fPIC $(INCLUDES) -o $@ -c $<

build/%.o: src/turtle/%.c
	@mkdir -p build
	@gcc $(CFLAGS) -fPIC $(INCLUDES) -o $@ -c $<

build/%.o: src/turtle/io/%.c
	@mkdir -p build
	@gcc $(CFLAGS) -fPIC $(INCLUDES) -o $@ -c $<

build/%.o: src/%.c include/%.h
	@mkdir -p build
	@gcc $(CFLAGS) -fPIC $(INCLUDES) -o $@ -c $<

build/%.o: src/deps/%.c src/deps/%.h
	@mkdir -p build
	@gcc $(CFLAGS) -fPIC -o $@ -c $<

# Rules for building the examples
examples: bin/example-demo bin/example-projection bin/example-pthread          \
          bin/example-stepper

bin/example-pthread: examples/example-pthread.c lib/libturtle.$(SOEXT)
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -Iinclude $< -Llib $(RPATH) -lturtle \
             -lpthread

bin/example-%: examples/example-%.c lib/libturtle.$(SOEXT)
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -Iinclude $< -Llib $(RPATH) -lturtle


# Rules for installing `libcheck` locally
CHECK_INSTALL_DIR := share/check

libcheck: $(CHECK_INSTALL_DIR)

$(CHECK_INSTALL_DIR):
	@echo "Installing libcheck to $@"
	@mkdir -p $@
	@git clone https://github.com/libcheck/check.git $@/src
	@cd $@/src && git checkout 6f6910e && cd -
	@mkdir -p $@/build
	@cd $@/build && cmake -DCMAKE_INSTALL_PREFIX=$(PWD)/$@                 \
		-DCHECK_ENABLE_TESTS=off ../src

	@cd $@/build && make install
	@echo "libcheck has been installed to $@"


# Rules for building the tests binaries
SOURCES := src/turtle/client.c src/turtle/ecef.c src/turtle/error.c            \
	src/turtle/io.c src/turtle/list.c src/turtle/map.c                     \
	src/turtle/projection.c src/turtle/stack.c src/turtle/stepper.c        \
	src/turtle/io/geotiff16.c src/turtle/io/grd.c src/turtle/io/hgt.c      \
	src/turtle/io/png16.c src/turtle/io/asc.c

test: bin/test-turtle
	@mkdir -p tests/topography
	@./bin/test-turtle
	@rm -rf tests/*.png tests/*.grd tests/*.hgt tests/*.tif tests/*.asc    \
		tests/topography/*
	@mv *.gcda tests/. 2> /dev/null || true
	@mv bin/*.gcda tests/. 2> /dev/null || true
	@gcov -o tests $(SOURCES) | tail -1
	@rm -rf tinydir.h.gcov tests/test-turtle.gcno tests/test-turtle.gcda
	@mv *.gcov tests/. 2> /dev/null || true
	@mv bin/*.gcov tests/. 2> /dev/null || true

ifneq ("$(wildcard $(CHECK_INSTALL_DIR))","")
bin/test-%: LIBS += -L$(CHECK_INSTALL_DIR)/lib -Wl,-rpath,$(CHECK_INSTALL_DIR)/lib
bin/test-%: INCLUDES += -I$(CHECK_INSTALL_DIR)/include
endif
bin/test-%: tests/test-%.c build/jsmn.o build/tinydir.o $(SOURCES)
	@mkdir -p bin
	@gcc -o $@ $(CFLAGS) -O0 -g --coverage -dumpbase '' $(INCLUDES) \
		$^ $(LIBS) -lcheck -lrt
	@mv *.gcno tests/. 2> /dev/null || true
	@mv bin/*.gcno tests/. 2> /dev/null || true


# Clean-up rule
clean:
	@rm -rf bin lib build tests/*.gcno tests/*.gcda tests/*.gcov *.gcov    \
		*.gcno *.gcda tests/*.png tests/*.grd tests/*.hgt tests/*.tif  \
		tests/*.asc tests/topography

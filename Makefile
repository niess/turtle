PACKAGES := libpng12 libxml-2.0
CFLAGS := -O2 -g -std=c99 -pedantic -fPIC $(shell pkg-config --cflags $(PACKAGES))
LIBS := -lm -ltiff $(shell pkg-config --libs $(PACKAGES))
INC := -Iinclude
OBJS := topo.o geotiff16.o

.PHONY: example lib clean

lib: lib/libtopo.so
	@rm -f *.o

clean:
	@rm -rf example lib *.o
	
lib/libtopo.so: $(OBJS)
	@mkdir -p lib
	@gcc -o $@ $(CFLAGS) -shared $(INC) $(OBJS) $(LIBS)
	
example: src/example.c	
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -ltopo
	
%.o: src/%.c include/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

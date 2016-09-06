PACKAGES := libpng12 libxml-2.0
CFLAGS := -O2 -std=c99 -pedantic -fPIC $(shell pkg-config --cflags $(PACKAGES))
LIBS := -lm -ltiff $(shell pkg-config --libs $(PACKAGES))
INC := -Iinclude
OBJS := turtle.o turtle_projection.o turtle_map.o turtle_datum.o turtle_client.o geotiff16.o

.PHONY: lib clean

lib: lib/libturtle.so
	@rm -f *.o

clean:
	@rm -rf example example-pthread lib *.o

lib/libturtle.so: $(OBJS)
	@mkdir -p lib
	@gcc -o $@ $(CFLAGS) -shared $(INC) $(OBJS) $(LIBS)

example: src/example.c
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle

example-pthread: src/example-pthread.c
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle -lpthread

%.o: src/%.c include/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<

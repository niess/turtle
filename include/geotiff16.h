/*
 * Basic interface to geotiff files providing a reader for 16b data.
 */
#ifndef GEOTIFF16_H
#define GEOTIFF16_H

#include <stdint.h>
#include <tiffio.h>

/* Data for reading a geotiff 16b file. */
struct geotiff16_reader {
	uint32_t width, height;
	TIFF * tiff;
};

/* Register the geotiff tags to libtiff. */
void geotiff16_register();

/* Manage a geotiff 16b file reader. */
int geotiff16_open(const char* path, struct geotiff16_reader * reader);
void geotiff16_close(struct geotiff16_reader * reader);
int geotiff16_readinto(struct geotiff16_reader * reader, int16_t * buffer);

#endif

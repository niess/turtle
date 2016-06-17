#ifndef GEOTIFF16_H
#define GEOTIFF16_H

#include <stdint.h>

typedef struct {
	uint32_t width;
	uint32_t height;
	int16_t elevation[]; /* Placeholder for data */
} Geotiff16Type_Map;

int Geotiff16_Initialise();
void Geotiff16_Finalise();

Geotiff16Type_Map* Geotiff16_NewMap(const char* path);
void Geotiff16_DeleteMap(Geotiff16Type_Map* map);

#endif

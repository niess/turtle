#include "geotiff16.h"
#include <tiffio.h>

/* Public GEOTIFF tags */
#define TIFFTAG_GEOPIXELSCALE		33550
#define TIFFTAG_INTERGRAPH_MATRIX	33920
#define TIFFTAG_GEOTIEPOINTS		33922
#define TIFFTAG_GEOKEYDIRECTORY		34735
#define TIFFTAG_GEODOUBLEPARAMS		34736
#define TIFFTAG_GEOASCIIPARAMS		34737

static const TIFFFieldInfo tiffFieldInfo[] = 
{  
	{ TIFFTAG_GEOPIXELSCALE,		-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM, 1, 1,	"GeoPixelScale"						},
	{ TIFFTAG_INTERGRAPH_MATRIX,	-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM, 1, 1,	"Intergraph TransformationMatrix"	},
	{ TIFFTAG_GEOTIEPOINTS,			-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM, 1, 1,	"GeoTiePoints"						},
	{ TIFFTAG_GEOKEYDIRECTORY,		-1,-1, TIFF_SHORT,	FIELD_CUSTOM, 1, 1,	"GeoKeyDirectory"					},
	{ TIFFTAG_GEODOUBLEPARAMS,		-1,-1, TIFF_DOUBLE,	FIELD_CUSTOM, 1, 1,	"GeoDoubleParams"					},
	{ TIFFTAG_GEOASCIIPARAMS,		-1,-1, TIFF_ASCII,	FIELD_CUSTOM, 1, 0,	"GeoASCIIParams"					},
};

/* Extender for GEOTIFF tags */
static TIFFExtendProc ParentExtender = NULL;
static void DefaultDirectory(TIFF *tiff)
{
	TIFFMergeFieldInfo(tiff, tiffFieldInfo, sizeof(tiffFieldInfo)/sizeof(tiffFieldInfo[0]));
	if (ParentExtender) 
		(*ParentExtender)(tiff);
}

/* Initialise the API by registering the tag extender to libtiff */
int Geotiff16_Initialise()
{
	ParentExtender = TIFFSetTagExtender(DefaultDirectory);
	return 0;
}

/* Finalise the API: empty for now */
void Geotiff16_Finalise()
{}

Geotiff16Type_Map* Geotiff16_NewMap(const char* path)
/* Load the elevation map from a GEOTIFF-16 file */
{
	Geotiff16Type_Map* map = NULL;
		
	/* Open the TIFF file */
	TIFF* tiff = TIFFOpen(path, "r");
	if (tiff == NULL)
		goto raise_error;
		
	/* Allocate memory for the data */
    uint32 width;
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &width);
    tsize_t size = TIFFScanlineSize(tiff);
    uint32 height = size/sizeof(int16);
    map = (Geotiff16Type_Map*)_TIFFmalloc(sizeof(Geotiff16Type_Map)+width*size);
    if (map == NULL)
		goto raise_error;
	map->width = width;
    map->height = height;
    
    /* Read the data */
    uint32 row, col;
    char* ptr = (char*)map->elevation+size*(width-1);	/* Data are stored from top to bottom, for runtime update	*/
    for (row = 0; row < width; row++)					/* We reverse back to matrix order							*/
    {
        if (TIFFReadScanline(tiff, ptr, row, 0) == -1)
			goto raise_error;
        ptr -= size;
    }
   
	goto clean_exit;
raise_error:
	Geotiff16_DeleteMap(map);
	
clean_exit:
	if (tiff != NULL)
		TIFFClose(tiff);
	
	return map;
}

void Geotiff16_DeleteMap(Geotiff16Type_Map* map)
/* Free the elevation map */
{
	if (map != NULL)
	{
		_TIFFfree(map);
		map = NULL;
	}
}	

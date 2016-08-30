/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#include <libxml/parser.h>

#include "turtle.h"
#include "geotiff16.h"

/* Initialise the TURTLE interface. */
enum turtle_return turtle_initialise(void)
{
	/* Initialise the libxml2 parser. */
	xmlInitParser();

	/* Initialise GEOTIFF-16. */
	return (Geotiff16_Initialise() == 0) ? TURTLE_RETURN_SUCCESS :
		TURTLE_RETURN_GEOTIFF_ERROR;
}

/* Clear the TURTLE interface. BEWARE: due to libxml2 this function must
 * be called only once.
 */
void turtle_finalise(void)
{
	/* Clear GEOTIFF-16 data. */
	Geotiff16_Finalise();

	/* Clear remanent XML paser data. */
	xmlCleanupParser();
}

/* Get a return code as a string. */
const char * turtle_strerror(enum turtle_return rc)
{
	static const char * msg[N_TURTLE_RETURNS] = {
		"Operation succeeded",
		"Bad address",
		"Bad file extension",
		"Bad file format",
		"No such file or directory",
		"Unknown projection",
		"Bad XML header",
		"Value is out of bound",
		"Couldn't initialise geotiff",
		"Not enough memory"
	};

	if ((rc < 0) || (rc >= N_TURTLE_RETURNS)) return NULL;
	else return msg[rc];
}

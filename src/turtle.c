/*
 *  Topographic Utilities for Rendering The eLEvation (TURTLE)
 *  Copyright (C) 2016  Valentin Niess
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * General purpose routines for the Turtle C library.
 */
#include <libxml/parser.h>

#include "turtle.h"
#include "geotiff16.h"

/* Initialise the TURTLE interface. */
void turtle_initialise(void)
{
	/* Initialise the libxml2 parser. */
	xmlInitParser();

	/* Register the geotiff16 tags. */
	geotiff16_register();
}

/* Clear the TURTLE interface. BEWARE: due to libxml2 this function must
 * be called only once.
 */
void turtle_finalise(void)
{
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
		"An internal error occured",
		"Couldn't lock",
		"Not enough memory",
		"Couldn't unlock"
	};

	if ((rc < 0) || (rc >= N_TURTLE_RETURNS)) return NULL;
	else return msg[rc];
}

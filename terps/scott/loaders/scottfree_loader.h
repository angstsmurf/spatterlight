/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scottfree_loader.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for the plain text ScottFree database format (.dat files).
 */

#ifndef scottfree_loader_h
#define scottfree_loader_h

#include <stdio.h>

#include "scott_defines.h"

GameIDType LoadDatabase(FILE *f);

#endif /* scottfree_loader_h */

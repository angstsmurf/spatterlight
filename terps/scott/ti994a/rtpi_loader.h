/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  rtpi_loader.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for "Return to Pirate's Isle" (RTPI) for the TI-99/4A.
 */

#ifndef rtpi_loader_h
#define rtpi_loader_h

#include <stddef.h>
#include <stdint.h>

#include "scott_defines.h"

GameIDType DetectRTPI(uint8_t *data, size_t datalength);
void UpdateRTPISystemMessages(void);

#endif /* rtpi_loader_h */

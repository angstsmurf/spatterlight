/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  sysmess_maps.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Maps each port's table of system messages to the sys[] slots of the
 *  interpreter.
 */

#ifndef sysmess_maps_h
#define sysmess_maps_h

#include <stddef.h>

#include "scott_defines.h"

void MapSysMessages(const SysMessageType *keys, size_t n, int src_offset);
int MapSysRange(SysMessageType first, SysMessageType last, int src);

/* Map system_messages[offset...] to sys[] through a whole key table */
#define MAP_SYS_MESSAGES(keys, offset) \
    MapSysMessages((keys), sizeof(keys) / sizeof((keys)[0]), (offset))

void Spiderman64Sysmess(void);
void SpidermanAtari8Sysmess(void);
void Adventureland64Sysmess(void);
void Claymorgue64Sysmess(void);
void Mysterious64Sysmess(void);
void PerseusItalianSysmess(void);
void Supergran64Sysmess(void);
void SecretMission64Sysmess(void);

#endif /* sysmess_maps_h */

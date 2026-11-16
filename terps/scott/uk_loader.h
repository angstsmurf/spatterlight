/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  uk_loader.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for the binary database format of the Adventure International UK
 *  (Brian Howarth) games.
 */

#ifndef uk_loader_h
#define uk_loader_h

#include <stdint.h>

#include "scott.h"

int SeekIfNeeded(int expected_start, size_t *offset, uint8_t **ptr);
GameIDType TryLoading(uint8_t *data, size_t datasize, const GameInfo *info, int dict_start);
DictionaryType GetId(const uint8_t *data, size_t datasize, size_t *offset);
int FindCode(const uint8_t *data, size_t datasize, const char *pattern, int patternLen);
uint8_t *ReadHeader(uint8_t *ptr);
int ParseHeader(int *h, HeaderType type, Header *out);
void PrintHeaderInfo(int *h, const Header *hdr);
uint8_t *SeekToPos(int offset);

int ReadRoomExits(uint8_t **ptr, int num_rooms, const uint8_t *endptr);
int ReadItemLocations(uint8_t **ptr, int num_items, const uint8_t *endptr);

extern int header[];

#endif /* uk_loader_h */

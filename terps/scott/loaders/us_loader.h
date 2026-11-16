/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  us_loader.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loader for the binary database format of the US (SAGA) games.
 */

#ifndef us_loader_h
#define us_loader_h

#include <stddef.h>
#include <stdint.h>

#include "scott_defines.h"

GameIDType FreeGameResources(void);

GameIDType LoadBinaryDatabase(uint8_t *data, size_t length, GameInfo info, int dict_start);

uint8_t *ParseRooms(uint8_t *ptr, uint8_t *endptr, int number_of_rooms);

uint8_t *ParseRoomConnections(uint8_t *ptr, uint8_t *endptr, int number_of_rooms, int stride);

uint8_t *ParseItems(uint8_t *ptr, uint8_t *endptr, int number_of_items, int stride);

uint8_t *ParseMessages(uint8_t *ptr, uint8_t *endptr, int number_of_messages);

uint8_t *ParseActions(uint8_t *ptr, uint8_t *data, size_t datalength, int number_of_actions, int big_endian);

uint8_t *Skip(uint8_t *ptr, int count, uint8_t *eof);

#endif /* us_loader_h */

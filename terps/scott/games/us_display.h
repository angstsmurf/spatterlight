/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  us_display.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Room and inventory pictures of the US (SAGA) games.
 */

#ifndef us_display_h
#define us_display_h

void InventoryUS(void);
int DrawUSRoom(int room);
int DrawFuzzyRoom(int room);
void DrawUSRoomObject(int item);
void LookUS(void);

#endif /* us_display_h */

/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  closeups.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The table of close-up pictures, shown when the player examines something.
 */

#ifndef closeups_h
#define closeups_h

#include "scott_defines.h"

void ShowCloseup(int image);
void ShowUSCloseup(int image, int offset);
void SetCloseupVerb(int verb, int us_pictures);
void AddCloseup(int item, int noun, int image);
int HasCloseupTable(void);
int GetCloseup(int index, int *verb, int *item, int *noun, int *image);
void ShowCloseupOnExamine(int verb, int noun);
extern int closeups_without_graphics;
void AddExtraCloseups(GameIDType game);

#endif /* closeups_h */

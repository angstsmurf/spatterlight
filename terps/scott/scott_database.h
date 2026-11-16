/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  scott_database.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The game database in memory, shared by the loaders and the engine.
 */

#ifndef scott_database_h
#define scott_database_h

#include "scott.h"

/* Copies parsed header values into GameHeader and refills the light source */
void SetGameHeader(const Header *h);
/* Allocates the database arrays from the counts in GameHeader */
void AllocateGameData(void);
void FreeDatabase(void);

const char *MapSynonym(int noun);
int MatchUpItem(int noun, int loc);
void ParseItemSlashAutoGet(int index);

#endif /* scott_database_h */

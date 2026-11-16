/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  detect_game.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-01-10.
 */

#ifndef detect_game_h
#define detect_game_h

#include "scott.h"
#include <stdint.h>

GameIDType DetectGame(const char *file_name);

#endif /* detect_game_h */

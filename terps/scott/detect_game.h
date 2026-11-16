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

/* Path of the game file and the directory it is in, where companion files
   are looked for */
extern const char *game_file;
extern char *DirPath;

/* The raw game file in memory, and the offset to add to the addresses of
   the game table when the file has extra data prepended */
extern uint8_t *entire_file;
extern size_t file_length;
extern int file_baseline_offset;

/* The message strings as read from the game file, before they are mapped
   to sys[] */
extern const char *system_messages[];

/* Title screen text and ZX Spectrum SCREEN$ loading picture, or NULL */
extern char *title_screen;
extern uint8_t *ZXLoadingScreen;

#endif /* detect_game_h */

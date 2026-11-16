/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  game_dispatch.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The places where the interpreter core hands over to the code of one
 *  Adventure International UK game: setup after loading, the game-specific
 *  opcode, the room picture and the timer animations.
 */

#ifndef game_dispatch_h
#define game_dispatch_h

#include "scott_defines.h"

/* Set up the system messages, extra data, image patches and graphics of a
   detected Adventure International UK / Brian Howarth or C64 game. */
void GameSetup(GameIDType detectedGame);

/* Run the game-specific opcode of the current game with the parameter p. */
void GameOpcode(int p);

/* Draw the room picture if the current game has its own way of doing it.
   Returns 1 if it did. */
int GameRoomImage(void);

/* Advance the timer animation of the current game, if it has one. Returns 1
   if it does. */
int GameTimer(void);

#endif /* game_dispatch_h */

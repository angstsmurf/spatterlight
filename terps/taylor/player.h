//
//  player.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2026-05-18.
//

#ifndef player_h
#define player_h

#include "taylor.h"

int LoadGame(void);
void SaveGame(void);
void Look(void);
void Inventory(void);
int YesOrNo(void);

/* Queries and updates of the object/flag state. */
unsigned char Destroyed(void);
unsigned char Carried(void);
unsigned char Worn(void);
unsigned char NumObjects(void);
int DarkFlag(void);
int CarryItem(void);
void DropItem(void);
void Put(unsigned char obj, unsigned char loc);
int Present(unsigned char obj);

extern uint8_t Flag[];
extern uint8_t ObjectLoc[];
extern int PrintedOK;
extern int Redraw;
extern int StopTime;
extern int JustStarted;
extern int ShouldRestart;
extern int NoGraphics;
extern char DelimiterChar;
extern int LastVerb;
extern GameInfo *Game;
extern int InKaylethPreview;

#endif /* player_h */

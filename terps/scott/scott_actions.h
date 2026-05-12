/*
 *  scott_actions.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Declarations for game state operations and the action engine.
 */

#ifndef scott_actions_h
#define scott_actions_h

#include "scottdefines.h"

int CountCarried(void);
void MoveItemAToLocOfItemB(int itemA, int itemB);
void GoTo(int loc);
void GoToStoredLoc(void);
void SetBitFlag(int bit);
void ClearBitFlag(int bit);
void SetDark(void);
void SetLight(void);
void SwapLocAndRoomflag(int index);
void SwapItemLocations(int itemA, int itemB);
void PutItemAInRoomB(int itemA, int roomB);
void SwapCounters(int index);
void PlayerIsDead(void);
ExplicitResultType PerformActions(int vb, int no);

#endif /* scott_actions_h */

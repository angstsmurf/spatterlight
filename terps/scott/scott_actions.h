/*
 *  scott_actions.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Declarations for game state operations and the action engine.
 */

#ifndef scott_actions_h
#define scott_actions_h

#include "scott_defines.h"

void ClearBitFlag(int bit);
void ClearScreen(void);
int CountCarried(void);
void Delay(float seconds);
void GameOver(void);
void GoTo(int loc);
void GoToStoredLoc(void);
void MoveItemAToLocOfItemB(int itemA, int itemB);
ExplicitResultType PerformActions(int vb, int no);
void OutputNumber(int a);
void PlayerIsDead(void);
void PrintMessage(int index);
void PrintNoun(void);
int PrintScore(void);
void PutItemAInRoomB(int itemA, int roomB);
int RandomPercent(int n);
void SetBitFlag(int bit);
void SetDark(void);
void SetLight(void);
void SwapCounters(int index);
void SwapItemLocations(int itemA, int itemB);
void SwapLocAndRoomflag(int index);

#endif /* scott_actions_h */

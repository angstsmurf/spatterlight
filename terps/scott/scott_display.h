/*
 *  scott_display.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Declarations for room description display, image rendering,
 *  inventory listing, and related UI functions.
 */

#ifndef scott_display_h
#define scott_display_h

#include "glk.h"

void UpdateUSInventory(void);
int ItIsDark(void);
void DrawBlack(void);
void OpenTopWindow(void);
glui32 OptimalPictureSize(glui32 graphwidth, glui32 graphheight, glui32 *outwidth, glui32 *outheight);
void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);
void DrawImage(int image);
void DrawRoomImage(void);
void Look(void);
void ListInventory(int upper);
void LookWithPause(void);

extern int print_look_to_transcript;

#endif /* scott_display_h */

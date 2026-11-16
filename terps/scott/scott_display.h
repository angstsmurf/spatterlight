/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

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
void DrawRoomImage(void);
int ItIsDark(void);
void DrawBlack(void);
void OpenBottomWindow(void);
void OpenTopWindow(void);
void CloseTopWindow(void);
glui32 OptimalPictureSize(glui32 graphwidth, glui32 graphheight, glui32 *outwidth, glui32 *outheight);
void FitPictureToGraphicsWindow(glui32 *graphheight, glui32 *optimal_height);
int GraphicsEnabled(void);
void OpenGraphicsWindow(void);
void CloseGraphicsWindow(void);
void DrawImage(int image);
void DrawRoomItemImages(void);
void Look(void);
void ListInventory(int upper);
void LookWithPause(void);
void RequestRedraw(void);

/* The windows, and the size of the top one in characters */
extern winid_t Bottom, Top, Graphics;
extern glui32 TopWidth, TopHeight;
extern int split_screen;

/* Size of the room pictures in pixels */
extern int ImageWidth;
extern int ImageHeight;

/* What is on screen now, to avoid redundant redraws */
extern int showing_inventory;
extern int showing_closeup;
extern int last_image_index;
extern int lastwasnewline;
extern int should_draw_image;

extern int should_look_in_transcript;
extern int print_look_to_transcript;

#endif /* scott_display_h */

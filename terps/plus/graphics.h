//
//  graphics.h
//  Plus
//
//  Created by Administrator on 2022-06-04.
//

#ifndef graphics_h
#define graphics_h

#include <stdio.h>

extern winid_t Graphics;

extern int pixel_size;
extern int x_offset, y_offset, right_margin;

int DrawCloseup(int item);
void DrawCurrentRoom(void);
void DrawRoomImage(int room);
void DrawItemImage(int item);
int DrawImageWithFilename(char *filename);
int DrawImageFromFile(char *filename);

#endif /* graphics_h */

//
//  apple2draw.h
//
//  Apple 2 bitmap graphics drawing for ScottFree and Plus,
//  two interpreters for adventures in Scott Adams formats
//
//  Created by Petter Sjölund on 2022-09-17.
//

#ifndef apple2draw_h
#define apple2draw_h

#include <stdint.h>
#include <stdlib.h>

void ClearApple2ScreenMem(void);
void DrawApple2ImageFromVideoMem(void);
void DrawApple2ImageFromVideoMemWithFlip(int upside_down);
typedef void(*adjust_plus_fn)(uint8_t, uint8_t, uint8_t);
int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize, int is_the_count, adjust_plus_fn adjust_plus);

extern uint8_t *descrambletable;

#endif /* apple2draw_h */

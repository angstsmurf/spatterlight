//
//  apple2draw.h
//  Spatterlight
//
//  Created by Administrator on 2022-09-17.
//

#ifndef apple2draw_h
#define apple2draw_h

void ClearApple2ScreenMem(void);
void DrawApple2ImageFromVideoMem(void);
int DrawApple2ImageFromData(uint8_t *ptr, size_t datasize);

#endif /* apple2draw_h */

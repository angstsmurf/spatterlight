//
//  sagadrawglue.h
//  plus
//
//  Created by Administrator on 2025-12-15.
//

#ifndef sagadrawglue_h
#define sagadrawglue_h

int C64A8AdjustPlus(int width, int height, int *x_origin);
void Apple2AdjustPlus(uint8_t width, uint8_t height, uint8_t Y_origin);

void TransC64ColorPlus(uint8_t index, uint8_t value);
void TransAtariColorPlus(uint8_t index, uint8_t value);

#endif /* sagadrawglue_h */

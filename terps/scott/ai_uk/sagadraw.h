//
//  sagadraw.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2021-12-15.
//

#ifndef sagadraw_h
#define sagadraw_h

#include "glk.h"
#include "scottdefines.h"

void SagaSetup(size_t imgoffset);
void RectFill(int32_t x, int32_t y, int32_t width, int32_t height,
    int32_t color);

#endif /* sagadraw_h */

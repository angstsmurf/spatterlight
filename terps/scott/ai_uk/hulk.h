//
//  hulk.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-18.
//

#ifndef hulk_h
#define hulk_h

#include "scott_defines.h"

void HulkShowImageOnExamine(int noun);
void HulkLook(void);
void InventoryUS(void);
void DrawHulkImage(int p);
int LoadDOSImages(void);

#endif /* hulk_h */

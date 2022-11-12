//
//  hulk.h
//  scott
//
//  Created by Administrator on 2022-01-18.
//

#ifndef hulk_h
#define hulk_h

#include <stdio.h>

void HulkShowImageOnExamine(int noun);
void CountShowImageOnExamineUS(int noun);
void VoodooShowImageOnExamineUS(int noun);
void HulkLook(void);
void InventoryUS(void);
void DrawHulkImage(int p);
int LoadBinaryDatabase(uint8_t *data, size_t length, struct GameInfo info, int dict_start);
int LoadDOSImages(void);

#endif /* hulk_h */

//
//  utility.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-10.
//

#ifndef utility_h
#define utility_h

#include "debugprint.h"
#include "glk.h"
#include "read_le16.h"
#include "memory_allocation.h"
#include "minmax.h"
#include "common_defines.h"
#include "common_utils.h"

void Display(winid_t w, const char *fmt, ...);
void CleanupAndExit(void);
uint8_t *SeekToPos(size_t offset);

int rotate_left_with_carry(uint8_t *byte, int last_carry);
int rotate_right_with_carry(uint8_t *byte, int last_carry);

extern uint8_t *FileImage;
extern size_t FileImageLen;
extern uint8_t *EndOfData;
extern strid_t Transcript;

#endif /* utility_h */

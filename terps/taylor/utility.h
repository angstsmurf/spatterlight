//
//  utility.h
//  taylor
//
//  Created by Administrator on 2022-03-10.
//

#ifndef utility_h
#define utility_h

#include <stdio.h>
#include "glk.h"

void Display(winid_t w, const char *fmt, ...);
void CleanupAndExit(void);
void Fatal(const char *x);
void *MemAlloc(int size);
uint8_t *SeekToPos(uint8_t *buf, size_t offset);
void print_memory(int address, int length);

extern uint8_t *FileImage;
extern size_t FileImageLen;
extern uint8_t *EndOfData;

#endif /* utility_h */

//
//  utility.h
//  taylor
//
//  Created by Administrator on 2022-03-10.
//

#ifndef utility_h
#define utility_h

#include <stdio.h>

void Display(winid_t w, const char *fmt, ...);
void CleanupAndExit(void);
void Fatal(const char *x);
void *MemAlloc(int size);
uint8_t *SeekToPos(uint8_t *buf, size_t offset);

extern uint8_t FileImage[];
extern size_t FileImageLen;

#endif /* utility_h */

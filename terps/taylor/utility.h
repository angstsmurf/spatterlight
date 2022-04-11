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
uint8_t *readFile(const char *name, size_t *size);
size_t writeToFile(const char *name, uint8_t *data, size_t size);

int rotate_left_with_carry(uint8_t *byte, int last_carry);
int rotate_right_with_carry(uint8_t *byte, int last_carry);

extern uint8_t *FileImage;
extern size_t FileImageLen;
extern uint8_t *EndOfData;

#endif /* utility_h */

//
//  utility.c
//  taylor
//
//  Created by Administrator on 2022-03-10.
//
#include <stdlib.h>

#include "glk.h"
#include "taylor.h"
#include "utility.h"

strid_t Transcript = NULL;

extern winid_t Bottom, Top;

void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

void Fatal(const char *x)
{
    Display(Bottom, "%s\n", x);
    CleanupAndExit();
}

void *MemAlloc(int size)
{
    void *t = (void *)malloc(size);
    if (t == NULL)
        Fatal("Out of memory");
    return (t);
}

uint8_t *SeekToPos(uint8_t *buf, size_t offset)
{
    if (offset > FileImageLen)
        return 0;
    return buf + offset;
}

void print_memory(int address, int length)
{
    unsigned char *p = FileImage + address;
    for (int i = address; i <= address + length ; i += 16){
        fprintf(stderr, "\n%04X:  ", i);
        for (int j = 0; j < 16; j++){
            fprintf(stderr, "%02X ", *p++);
        }
    }
}


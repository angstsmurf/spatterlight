//
//  memory_allocation.c
//  Spatterlight
//
//  Created by Administrator on 2025-12-20.
//

#include <stdlib.h>

#include "memory_allocation.h"

void Fatal(const char *x);

void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == NULL)
        Fatal("Out of memory");
    return (t);
}

void *MemCalloc(size_t size)
{
    void *t = (void *)calloc(1, size);
    if (t == NULL)
        Fatal("Out of memory");
    return (t);
}

void *MemRealloc(void *ptr, size_t size)
{
    void *t = (void *)realloc(ptr, size);
    if (t == NULL)
        Fatal("Out of memory");
    return (t);
}

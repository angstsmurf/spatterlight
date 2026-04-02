//
//  memory_allocation.cpp
//  bocfel
//
//  Created by Administrator on 2026-04-02.
//

#include <stdlib.h>
#include <stdio.h>

void *MemAlloc(size_t size)
{
    void *t = (void *)malloc(size);
    if (t == nullptr) {
        fprintf(stderr, "Out of memory");
        exit(1);
    }
    return (t);
}

void *MemCalloc(size_t size)
{
    void *t = (void *)calloc(1, size);
    if (t == nullptr) {
        fprintf(stderr, "Out of memory");
        exit(1);
    }

    return (t);
}

void *MemRealloc(void *ptr, size_t size)
{
    void *t = (void *)realloc(ptr, size);
    if (t == nullptr) {
        fprintf(stderr, "Out of memory");
        exit(1);
    }
    return (t);
}

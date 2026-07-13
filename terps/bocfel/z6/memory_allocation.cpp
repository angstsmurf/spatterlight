//
//  memory_allocation.cpp
//  bocfel
//
//  Created by Administrator on 2026-04-02.
//

#include <stdlib.h>
#include <stdio.h>

#include "v6_image.h"

size_t image_buffer_size(int width, int height, int bytes_per_pixel)
{
    if (width <= 0 || height <= 0)
        return 0;
    if (width > kMaxImageDimension || height > kMaxImageDimension)
        return 0;
    if (bytes_per_pixel != 1 && bytes_per_pixel != kBytesPerPixel)
        return 0;
    return (size_t)width * (size_t)height * (size_t)bytes_per_pixel;
}

uint8_t *image_alloc(int width, int height, int bytes_per_pixel, size_t *size_out)
{
    size_t size = image_buffer_size(width, height, bytes_per_pixel);
    if (size_out != nullptr)
        *size_out = 0;
    if (size == 0) {
        fprintf(stderr, "image_alloc: refusing %d x %d image (%d bytes per pixel)\n",
                width, height, bytes_per_pixel);
        return nullptr;
    }
    uint8_t *buffer = (uint8_t *)calloc(1, size);
    if (buffer == nullptr) {
        fprintf(stderr, "image_alloc: out of memory (%zu bytes)\n", size);
        return nullptr;
    }
    if (size_out != nullptr)
        *size_out = size;
    return buffer;
}

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

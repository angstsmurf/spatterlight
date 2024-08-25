//
//  extract_apple_2.h
//  bocfel6
//
//  Created by Administrator on 2023-07-16.
//
//  Based on ZCut by Stefan Jokisch

#ifndef extract_apple_2_h
#define extract_apple_2_h

#include <stdio.h>
#include "v6_image.h"

uint8_t *extract_apple_2(const char *filename, size_t *file_length, ImageStruct **raw_images, int *num_images, int *version);

#endif /* extract_apple_2_h */

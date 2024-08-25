//
//  decompress_vga.hpp
//  bocfel6
//
//  Created by Administrator on 2023-08-03.
//
//  Based on drilbo-mg1.h from Fizmo by Christoph Ender,
//  which in turn is based on Mark Howell's pix2gif utility

#ifndef decompress_vga_hpp
#define decompress_vga_hpp

#include <stdio.h>

#include <stdio.h>
#include "v6_image.h"

uint8_t *decompress_vga(ImageStruct *image);

#endif /* decompress_vga_hpp */

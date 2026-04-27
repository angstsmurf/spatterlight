//
//  decompress_vga.hpp
//  bocfel6
//
//  Created by Administrator on 2023-08-03.
//
//  Based on drilbo-mg1.h from Fizmo by Christoph Ender,
//  which in turn is based on Mark Howell's pix2gif utility
//
//  Implements LZW decompression for Infocom V6 VGA/EGA/CGA image data.
//  The compressed data uses variable-width codes (starting at 9 bits,
//  growing up to 12 bits) with a code table of up to 4096 entries.
//

#ifndef decompress_vga_hpp
#define decompress_vga_hpp

#include <stdio.h>

#include <stdio.h>
#include "v6_image.h"

uint8_t *decompress_vga(const ImageStruct *image);

#endif /* decompress_vga_hpp */

//
//  decompress_vga.hpp
//  bocfel6
//
//  Created by Administrator on 2023-08-03.
//

#ifndef decompress_vga_hpp
#define decompress_vga_hpp

#include "v6_image.h"

// LZW decompression of VGA/EGA/CGA image data to width * height palette
// indices; see decompress_vga.cpp. The caller frees the result. Returns
// nullptr on bad dimensions, allocation failure or corrupt data.
uint8_t *decompress_vga(const ImageStruct *image);

#endif /* decompress_vga_hpp */

//
//  extract_image_data.hpp
//  bocfel6
//
//  Created by Administrator on 2023-07-17.
//
//  Parts are based on Mark Howell's pix2gif utility

#ifndef extract_image_data_hpp
#define extract_image_data_hpp

#include <stdio.h>
#include "v6_image.h"

int extract_images(uint8_t *data, size_t filesize, int disk, off_t offset, ImageStruct **image_list, int *version, GraphicsType *type);
int extract_images_from_blorb(ImageStruct **image_list);

#endif /* extract_image_data_hpp */

//
//  writetiff.hpp
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//

#ifndef writetiff_hpp
#define writetiff_hpp


#include "io.h"
#include "types.h"

void writetiff(FILE *fptr, const uint8_t *pixarray, uint32_t pixarraysize, int width);

#endif /* writetiff_hpp */

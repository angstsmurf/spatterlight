//
//  writetiff.hpp
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//

#ifndef writetiff_hpp
#define writetiff_hpp

#include <stdio.h>

#include "io.h"
#include "types.h"

void writetiff(FILE *fptr, uint8_t *pixarray, uint32_t pixarraysize, int nx);

#endif /* writetiff_hpp */

//
//  writetiff.h
//  bocfel6
//
//  Created by Administrator on 2023-05-22.
//

#ifndef writetiff_h
#define writetiff_h

#include <_types/_uint32_t.h>
#include <_types/_uint8_t.h>

#include <stdio.h>

void writetiff(FILE *fptr, uint8_t *pixarray, uint32_t pixarraysize, int width);

#endif /* writetiff_h */

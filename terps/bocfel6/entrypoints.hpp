//
//  entrypoints.hpp
//  bocfel6
//
//  Created by Administrator on 2023-08-06.
//

#ifndef entrypoints_hpp
#define entrypoints_hpp

#include <stdio.h>

void find_entrypoints(int start, int end);
void check_entrypoints(uint32_t pc);

extern uint8_t fg_global_idx, bg_global_idx;

#endif /* entrypoints_hpp */
//
//  memory_allocation.hpp
//  bocfel
//
//  Created by Administrator on 2026-04-02.
//

#ifndef memory_allocation_h
#define memory_allocation_h

#include "types.h"

void *MemAlloc(size_t size);
void *MemCalloc(size_t size);
void *MemRealloc(void *ptr, size_t size);

#endif /* memory_allocation_hpp */

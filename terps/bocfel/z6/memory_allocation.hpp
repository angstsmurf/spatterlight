//
//  memory_allocation.hpp
//  bocfel
//
//  Created by Administrator on 2026-04-02.
//

#ifndef memory_allocation_hpp
#define memory_allocation_hpp

#include "types.h"

// malloc / calloc that exit(1) on failure, so callers need not check.
// (The overflow-checked image buffer allocator image_alloc(), also defined
// in memory_allocation.cpp, is declared in v6_image.h and returns nullptr
// instead.)
void *MemAlloc(size_t size);
void *MemCalloc(size_t size);

#endif /* memory_allocation_hpp */

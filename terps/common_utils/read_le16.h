//
//  read_le16.h
//  Spatterlight
//
//  Created by Administrator on 2025-12-16.
//

#ifndef read_le16_h
#define read_le16_h

#include <stdint.h>

uint16_t READ_LE_UINT16(const void *ptr);
uint16_t READ_LE_UINT16_AND_ADVANCE(uint8_t **ptr);
uint16_t READ_BE_UINT16_AND_ADVANCE(uint8_t **ptr);

#endif


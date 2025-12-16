//
//  read_le16.c
//  Spatterlight
//
//  Created by Administrator on 2025-12-16.
//

#include "read_le16.h"

inline uint16_t READ_LE_UINT16(const void *ptr) {
    const uint8_t *b = (const uint8_t *)ptr;
    return (b[1] << 8) | b[0];
}

inline uint16_t READ_LE_UINT16_AND_ADVANCE(uint8_t **ptr) {
    uint8_t *b = *ptr;
    (*ptr) += 2;
    return (b[1] << 8) | b[0];
}

inline uint16_t READ_BE_UINT16_AND_ADVANCE(uint8_t **ptr) {
    uint8_t *b = *ptr;
    (*ptr) += 2;
    return b[1] | (b[0] << 8) ;
}

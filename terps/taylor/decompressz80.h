//
//  decompressz80.h
//  Spatterlight
//
//  Created by Administrator on 2022-01-24.
//

#ifndef decompressz80_h
#define decompressz80_h

/* Will return NULL on error or 0xc000 (49152) bytes of uncompressed raw data on
 * success */
uint8_t *DecompressZ80(uint8_t *raw_data, size_t *length);
uint8_t *find_tap_block(int wantedindex, const uint8_t *buffer,
                        size_t *length);

uint8_t *GetTZXBlock(int blockno, uint8_t *srcbuf, size_t *length);

#endif /* decompressz80_h */

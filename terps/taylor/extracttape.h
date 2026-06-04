//
//  extracttape.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-04-11.
//

#ifndef extracttape_h
#define extracttape_h

#include <stdint.h>

/* GetTZXBlock and the Alkatraz primitives (ldir/lddr/DeAlkatraz/
   DeshuffleAlkatraz) now live in the shared decompressz80 library. */
#include "decompressz80.h"

/* Base address of the ZX Spectrum's writable RAM (and screen memory).
   ROM occupies 0x0000-0x3FFF; usable RAM starts at 0x4000, which is also
   where Taylor's games load. Used to translate between Z80 addresses
   stored in game data and offsets into our loaded FileImage. */
#define ZX_RAM_BASE 0x4000

uint8_t *ProcessFile(uint8_t *image, size_t *length);

#endif /* extracttape_h */

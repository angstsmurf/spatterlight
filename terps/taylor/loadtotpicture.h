//
//  loadtotpicture.h
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
//
//  Created by Petter Sjölund on 2022-04-19.
//

#ifndef loadtotpicture_h
#define loadtotpicture_h

#include <stddef.h>
#include <stdint.h>

#include "decompressz80.h"

/* Returns 0 if the file_length bytes at file are too few for the picture */
int LoadAlkatrazPicture(uint8_t *mem, const uint8_t *file, size_t file_length,
    const AlkatrazKey *schedule);

#endif /* loadtotpicture_h */

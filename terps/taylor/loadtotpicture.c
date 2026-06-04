//
//  loadtotpicture.c
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
//
//  Created by Petter Sjölund on 2022-04-19.
//

#include <stdint.h>
#include <stddef.h>

#include "alkatraz_screen.h"
#include "extracttape.h"
#include "loadtotpicture.h"
#include "utility.h"

#define SCREEN_MEMORY_SIZE 0x1b00

/* Public entry point: build the screen draw order and run DeAlkatraz() to
   decrypt the picture into memory (the memory array represents the entire
   addressable 64 KB of the ZX Spectrum).

   The original image loader reads a byte from tape, decrypts it and writes the
   result to a screen address from the draw order; the resulting image is then
   used to decrypt the game data. */
void LoadAlkatrazPicture(uint8_t *memory, uint8_t *file)
{
    /* Expand the loader's window descriptors (count at 0xeea9, descriptors at
       0xeeab) into the screen draw order. */
    uint16_t order[SCREEN_MEMORY_SIZE];
    int count = AlkatrazScreenOrder(memory + 0xeeab,
        READ_LE_UINT16(memory + 0xeea9), order, SCREEN_MEMORY_SIZE);

    /* Read each pixel/attribute byte from tape, decrypt it with the loader's
       rolling-key XOR, and write it to the next address in draw order. */
    uint8_t loacon = 0xf6;
    DeAlkatraz(file, memory, 0, 0xea7d, 2, &loacon, 0xc1, 0xcb, 1);
    size_t fileoffset = 2;
    for (int i = 0; i < count; i++)
        DeAlkatraz(file, memory, fileoffset++, order[i], 1, &loacon, 0xc1, 0xcb, 1);
}

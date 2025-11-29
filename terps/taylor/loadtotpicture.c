//
//  loadtotpicture.c
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
//  It is Z80 assembly translated to C in the most lazy way imaginable.
//
//  Created by Petter Sjölund on 2022-04-19.
//

#include <stdlib.h>
#include <stdbool.h>

#include "extracttape.h"
#include "loadtotpicture.h"
#include "utility.h"

#define SCREEN_MEMORY_SIZE 0x1b00
#define ATTRIBUTES_MEMORY_TOP 0x5800

static uint8_t *mem = NULL;
static uint16_t *ScreenAddresses = NULL;

/* Saved register snapshot */
typedef struct {
    uint8_t A;
    uint16_t BC;
    uint16_t DE;
    uint16_t HL;
    uint16_t IY;
} SavedRegs;

static SavedRegs Slots[3];

static inline bool even_parity(uint8_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return (__builtin_popcount((unsigned)x) & 1) == 0;
#else
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;
    return (x & 1) == 0;
#endif
}

/* Record a snapshot into the slot 'slotIdx' (1..3). */
static void save_slot(int slotIdx, uint8_t A, uint16_t BC, uint16_t DE, uint16_t HL, uint16_t IY)
{
    if (slotIdx < 0 || slotIdx > 2) {
        fprintf(stderr, "Internal error: invalid slot index %u\n", slotIdx);
        exit(1);
    }

    // slotIdx can be 1, 2 or 3, selecting a different "slot" at 0xee62, 0xee6d or 0xee78 respectively
    Slots[slotIdx].A = A;
    Slots[slotIdx].BC = BC;
    Slots[slotIdx].DE = DE;
    Slots[slotIdx].HL = HL;
    Slots[slotIdx].IY = IY;
}

/* Read a previously saved snapshot from slot 'slotIdx' (1..3). */
static uint8_t load_slot(int slotIdx, uint16_t *BC, uint16_t *DE, uint16_t *HL, uint16_t *IY)
{
    if (slotIdx < 0 || slotIdx > 2) {
        fprintf(stderr, "Internal error: invalid slot index %u\n", slotIdx);
        exit(1);
    }

    *BC = Slots[slotIdx].BC;
    *DE = Slots[slotIdx].DE;
    *HL = Slots[slotIdx].HL;
    *IY = Slots[slotIdx].IY;

    return Slots[slotIdx].A;
}

/* Add a new address to the screen addresses list array */
static int CurrentScreenOffset = 0;

static void add_to_array(uint16_t val)
{
    ScreenAddresses[CurrentScreenOffset++] = val;
}

/* Add another "attributes" address to the screen addresses list array */
static void add_attribute(uint16_t addr, uint8_t direction)
{
    if (!even_parity(direction))
        addr ^= 0xffff;
    if ((addr & 0x700) == 0)
        add_to_array(((addr >> 11) & 0x300) | ATTRIBUTES_MEMORY_TOP);
}

/* Here we get the address in screen memory representing one pixel down */
/* or up from addr, depending on the parity of the direction variable */
static uint16_t next_line_addr(uint16_t addr, uint8_t direction)
{
    if (even_parity(direction)) {
        /* pixel down */
        addr += 0x100;
        if ((addr & 0x700) != 0)
            return addr;
        addr += 0x20;
        if ((addr & 0xe0) == 0)
            return addr - 0x100;
        return addr - 0x800;
    } else {
        /* pixel up */
        addr -= 0x100;
        if (((addr ^ 0xffff) & 0x700) != 0)
            return addr;
        addr -= 0x20;
        if (((addr ^ 0xffff) & 0xe0) != 0)
            return addr + 0x800;
        return addr + 0x100;
    }
}

/* Determine the next direction (returns 0 or 0xff) and update registers. */
static uint8_t styler(uint8_t slotIdx, uint16_t *BC, uint16_t *DE, uint16_t *HL, uint16_t *IY)
{
    if (slotIdx < 0 || slotIdx > 2) {
        fprintf(stderr, "Error! Only slot values 0, 1, 2 allowed! (slot == %d)\n", slotIdx);
        exit(1);
    }

    /* load the saved snapshot for this slot */
    if (load_slot(slotIdx, BC, DE, HL, IY) == 0)
        return 0; /* A was zero in saved snapshot -> return 0 */

    // DIRECTION_OF_LOAD
    uint8_t direction = (uint8_t)(*IY >> 8);

    add_attribute(*HL, direction);
    add_to_array(*HL);

    *BC -= 0x100;

    if (direction < 2) {
        if ((*BC >> 8) != 0) {
            *HL = next_line_addr(*HL, direction);
            return 0xff;
        }
        uint8_t column = (uint8_t)(*BC & 0xff) + (even_parity(direction) ? 1 : -1);
        *HL = (uint16_t)((*DE & 0xff00) | column);
        *BC = (uint16_t)((*IY << 8) | column);
        *DE = (uint16_t)((*DE & 0xff) - 1);
        if (*DE == 0)
            return 0;
        return 0xff;
    }

    /* STYLE34 */
    uint8_t column = (uint8_t)(*HL & 0xff) + (even_parity(direction) ? 1 : -1);
    *HL = (uint16_t)((*HL & 0xff00) | column);

    if ((*BC >> 8) != 0) {
        return 0xff;
    }
    *HL = next_line_addr((uint16_t)((*HL & 0xff00) | (*BC & 0xff)), direction);
    *BC = (uint16_t)((*DE << 8) | (*HL & 0xff));
    *DE -= 0x100;
    if ((*DE >> 8) == 0)
        return 0;
    return 0xff;
}

/* Build the list of screen addresses used by the loader. */
static void address_table(void)
{
    uint16_t IY = 0x032d;
    uint16_t IX = 0xeeab;
    uint16_t DE, BC, HL;
    uint16_t counter = (uint16_t)(mem[0xeea9] | mem[0xeeaa] * 0x100);

    CurrentScreenOffset = 0;

    do { /* next line */
        uint8_t starting_slot = (uint8_t)(mem[IX + 3] >> 5);
        for (int slot = starting_slot; slot >= 0; slot--) {
            HL = (uint16_t)(mem[IX] | mem[IX + 1] * 0x100);
            DE = (uint16_t)((mem[IX + 2] & 0x3f) + (mem[IX + 3] & 0x1f) * 0x800);
            uint8_t A = (uint8_t)(mem[IX + 2] >> 6);
            IY = (uint16_t)(A * 0x100 | (IY & 0xff));
            BC = (uint16_t)((DE << 8) | (HL & 0xff));
            if (A < 2) {
                IY = (uint16_t)(A * 0x100 | (DE >> 8));
                BC = (uint16_t)((DE >> 8) * 0x100 | mem[IX]);
                DE = (uint16_t)(mem[IX + 1] * 0x100 | (DE & 0xff));
            }
            /* save into slot i (1..3). */
            save_slot(slot, A + 1, BC, DE, HL, IY);
            IX += 4;
            counter--;
        }

        uint8_t keep_going;
        do { /* NXTP1 loop */
            keep_going = 0;
            for (int slot = starting_slot; slot >= 0; slot--) {
                // styler() returns 0 or 0xff
                uint8_t styler_result = styler(slot, &BC, &DE, &HL, &IY);
                /* push the possibly-updated registers back into the slot */
				save_slot(slot, styler_result, BC, DE, HL, IY);
                keep_going += styler_result;
            }
        } while (keep_going != 0);

    } while (counter != 0);
}

/* Public entry point: builds the address table and runs DeAlkatraz
 to decrypt the picture into mem. */

/* The image loader reads a byte from tape, decrypts it and writes
 the result to a screen adress pulled from the list we create here */

uint8_t *LoadAlkatrazPicture(uint8_t *memimage, uint8_t *file)
{
    if (mem != NULL)
        free(mem);
    mem = memimage;
    ScreenAddresses = (uint16_t *)MemAlloc(SCREEN_MEMORY_SIZE * sizeof(uint16_t));

    address_table();

    uint8_t loacon = 0xf6;
    DeAlkatraz(file, mem, 0, 0xea7d, 2, &loacon, 0xc1, 0xcb, 1);
    size_t fileoffset = 2;
    for (int i = 0; i < SCREEN_MEMORY_SIZE; i++)
        DeAlkatraz(file, mem, fileoffset++, ScreenAddresses[i], 1, &loacon, 0xc1, 0xcb, 1);
    free(ScreenAddresses);
    ScreenAddresses = NULL;

    return mem;
}

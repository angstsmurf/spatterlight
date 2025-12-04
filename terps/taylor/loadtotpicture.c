//
//  loadtotpicture.c
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  This code is for de-protecting the ZX Spectrum text-only version of Temple of Terror
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

typedef enum {
    kDirRight = 0,
    kDirLeft = 1,
    kDirUp = 2,
    kDirDown = 3
} DirectionType;

static uint16_t *LookupTable = NULL;
static int TableWritePos = 0;

/* Saved register snapshot */
typedef struct {
    bool finished;
    uint8_t x_origin;
    uint8_t width;
    uint8_t remaining_x;
    uint8_t remaining_y;
    uint16_t current_addr;
    DirectionType direction;
} SavedRegs;

static SavedRegs Slots[3];

/* Record a snapshot into the slot (0..2). */
static void save_slot(int slot, bool finished, uint8_t x_origin, uint8_t width, uint8_t remaining_x, uint8_t remaining_y, uint16_t current_addr, DirectionType direction)
{
    if (slot < 0 || slot > 2) {
        fprintf(stderr, "Internal error: invalid slot index %u\n", slot);
        exit(1);
    }

    Slots[slot].finished = finished;
    Slots[slot].x_origin = x_origin;
    Slots[slot].width = width;
    Slots[slot].remaining_x = remaining_x;
    Slots[slot].remaining_y = remaining_y;
    Slots[slot].current_addr = current_addr;
    Slots[slot].direction = direction;
}

/* Read a previously saved state from slot 'slotIdx' (0..2). */
static bool load_slot(int slot, uint8_t *x_origin, uint8_t *width, uint8_t *remaining_x, uint8_t *remaining_y, uint16_t *current_addr, DirectionType *direction)
{
    if (slot < 0 || slot > 2) {
        fprintf(stderr, "Internal error: invalid slot index %u\n", slot);
        exit(1);
    }

    *current_addr = Slots[slot].current_addr;
    *direction = Slots[slot].direction;
    *x_origin = Slots[slot].x_origin;
    *width = Slots[slot].width;
    *remaining_x = Slots[slot].remaining_x;
    *remaining_y = Slots[slot].remaining_y;

    return Slots[slot].finished;
}

/* Add a new address to the screen addresses list array */
static void add_to_lookup_table(uint16_t val)
{
    if (TableWritePos >= SCREEN_MEMORY_SIZE) {
        fprintf(stderr, "loadtotpicture.c: lookup table write position out of bounds!\n");
        return;
    }
    LookupTable[TableWritePos++] = val;
}

/* Add an "attributes memory" address to the screen addresses lookup table */
static void add_attribute(uint16_t addr, DirectionType direction)
{
    /* Convert the addr screen address to
       its corresponding attributes memory address. */
    /* Only the most significant 9 bits change. */
    if (direction == kDirLeft || direction == kDirUp)
        addr ^= 0xffff;
    if ((addr & 0x700) == 0)
        add_to_lookup_table(((addr >> 11) & 0x300) | ATTRIBUTES_MEMORY_TOP);
}

/* Here we get the address in screen memory representing one pixel down */
/* or up from addr, depending on the parity of the direction variable. */

/* (Of course, an address represents *eight* horizontal pixels, */
/* but vertically the result is one pixel up or down.) */
static uint16_t next_line_addr(uint16_t addr, uint8_t direction)
{
    if (direction == kDirLeft || direction == kDirUp) {
        /* pixel up */
        addr -= 0x100;
        if (((addr ^ 0xffff) & 0x700) != 0)
            return addr;
        else
            addr -= 0x20;
        if (((addr ^ 0xffff) & 0xe0) != 0)
            return addr + 0x800;
        else
            return addr + 0x100;
    } else {
        /* pixel down */
        addr += 0x100;
        if ((addr & 0x700) != 0)
            return addr;
        else
            addr += 0x20;
        if ((addr & 0xe0) != 0)
            return addr - 0x800;
        else
            return addr - 0x100;
    }
}

/* Work out which screen memory address comes next in the lookup table and update variables. Returns true if this was the final address of the block in this slot. */
static bool calculate_address(uint8_t slot, uint8_t *x_origin, uint8_t *width, uint8_t *x_remaining, uint8_t *y_remaining, uint16_t *current_addr, DirectionType *direction)
{
    if (slot > 2) {
        fprintf(stderr, "Error! Only slot values 2 and below allowed! (slot == %d)\n", slot);
        exit(1);
    }

    /* load the saved snapshot for this slot */
    if (load_slot(slot, x_origin, width, x_remaining, y_remaining, current_addr, direction))
        return true; /* finished was true in saved snapshot -> return true */

    add_attribute(*current_addr, *direction);
    add_to_lookup_table(*current_addr);

    *x_remaining -= 1;

    if (*direction == kDirLeft || *direction == kDirRight) {
        /* direction is always 2 or 3 in Temple of Terror side B, so this code path is not implemented */
        fprintf(stderr, "TAYLOR: calculate_address: Unimplemented direction (%s).\n", *direction == kDirLeft ? "left" : "right");
        return true;
    } else {
        uint8_t low_byte = (uint8_t)(*current_addr & 0xff) + (*direction == kDirDown ? 1 : -1);
        *current_addr = (uint16_t)((*current_addr & 0xff00) | low_byte);

        if (*x_remaining != 0) {
            return false;  // keep going
        } else {
            *current_addr = next_line_addr((uint16_t)((*current_addr & 0xff00) | *x_origin), *direction);
            *x_remaining = *width;
            *x_origin = *current_addr & 0xff;
            *y_remaining -= 1;
            if (*y_remaining == 0)
                return true; // we have finished "drawing" the block
            else
                return false; // keep going
        }
    }
}

/* Build the list of screen addresses used by the loader. */
static void build_screen_address_lookup_table(uint8_t *mem)
{
    // counter is the total number of draw blocks, including simultaneous ones. Four bytes per block.
    uint16_t counter = (uint16_t)(mem[0xeea9] | mem[0xeeaa] * 0x100);
    if (counter * 4 + 0xeeab >= 0x10000) {
        fprintf(stderr, "build_screen_address_lookup_table: Bad input data. Instruction count is out of bounds!\n");
        return;
    }
    /* Instructions are encoded in (counter) consecutive groups of four bytes */
    uint8_t *instruction_bytes = &mem[0xeeab];
    uint16_t current_addr;
    DirectionType direction;
    uint8_t y_remaining = 0, x_remaining = 0, x_origin = 0, width = 0;

    TableWritePos = 0;

    do {
        /* Number (minus one) of simultaneous block drawing operations (0..3) is encoded in bits 7 to 6 of 4th instruction byte */
        uint8_t simultaneous = (uint8_t)(instruction_bytes[3] >> 5) + 1;

        /* We add a slot for each simultaneous block, by reading another four instruction bytes per block */
        for (int slot = 0; slot < simultaneous; slot++) {
            /* current_addr is the first word of instructions */
            current_addr = (uint16_t)(instruction_bytes[0] | instruction_bytes[1] * 0x100);

            /* direction is bits 7 to 6 of the third instruction byte */
            direction = (DirectionType)(instruction_bytes[2] >> 6);

            if (direction == kDirUp || direction == kDirDown) {
                /* If we are moving vertically, x size (in characters) */
                /* is given by the remainder (six rightmost bits) */
                /* of the third instruction byte  */
                x_remaining = instruction_bytes[2] & 0x3f;
                /* y size (in pixels) is the fourth instruction byte */
                /* times eight */
                y_remaining = instruction_bytes[3] * 8;
                x_origin = current_addr & 0xff;
                width = x_remaining;
            } else {
                /* direction is always kDirUp or kDirDown (2 or 3) in Temple of Terror side B, so this code path is unimplemented */
                fprintf(stderr, "TAYLOR: build_screen_address_lookup_table: Unimplemented direction (%s).\n", direction == kDirLeft ? "left" : "right");
            }

            /* save into slot (0..2). */
            save_slot(slot, false, x_origin, width, x_remaining, y_remaining, current_addr, direction);
            instruction_bytes += 4;
            counter--;
        }

        /* Now we repeat through the simultaneous block drawing operations until all are finished */
        bool keep_going = true;
        while (keep_going) {
            keep_going = false;
            for (int slot = 0; slot < simultaneous; slot++) {
                bool slot_finished = calculate_address(slot, &x_origin, &width, &x_remaining, &y_remaining, &current_addr, &direction);

                /* push the updated values back into the slot */
                /* This is easier to do here than inside the save_slot() function */
				save_slot(slot, slot_finished, x_origin, width, x_remaining, y_remaining, current_addr, direction);

                // Keep going until all simultaneous blocks are finished
                if (!slot_finished)
                    keep_going = true;
            }
        };

    } while (counter != 0);
}

/* Public entry point: builds an address lookup table and runs DeAlkatraz() */
/* to decrypt the picture into memory (the memory array represents the entire */
/* addressable 64 KB of the ZX Spectrum). */

/* The original image loader reads a byte from tape, decrypts it */
/* and writes the result to a screen address from the lookup table */
/* created here. The resulting image is then used to decrypt the game data. */

void LoadAlkatrazPicture(uint8_t *memory, uint8_t *file)
{
    if (LookupTable != NULL)
        free(LookupTable);

    LookupTable = (uint16_t *)MemAlloc(SCREEN_MEMORY_SIZE * sizeof(uint16_t));

    build_screen_address_lookup_table(memory);

    uint8_t loacon = 0xf6;
    DeAlkatraz(file, memory, 0, 0xea7d, 2, &loacon, 0xc1, 0xcb, 1);
    size_t fileoffset = 2;
    for (int i = 0; i < SCREEN_MEMORY_SIZE; i++)
        DeAlkatraz(file, memory, fileoffset++, LookupTable[i], 1, &loacon, 0xc1, 0xcb, 1);
    free(LookupTable);
    LookupTable = NULL;
}

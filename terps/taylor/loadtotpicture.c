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

#include "extracttape.h"
#include "loadtotpicture.h"
#include "utility.h"

#define SCREEN_MEMORY_SIZE 0x1b00

static uint8_t *mem;
static uint16_t *ScreenAddresses = NULL;
static int AddressIndex = 0;

static uint16_t push(uint16_t reg, uint16_t SP)
{
    SP -= 2;
    mem[SP] = reg & 0xff;
    mem[SP + 1] = reg >> 8;
    return SP;
}

static uint16_t pop(uint16_t *SP)
{
    uint16_t reg = mem[*SP] | mem[*SP + 1] * 256;
    *SP += 2;
    return reg;
}

static int parity(uint8_t x)
{
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;
    return (x & 1) == 0;
}

/* The image loader reads a byte from tape, decrypts it and writes the result */
/* to a screen adress pulled from the list we create here */

/* Add a new address to the screen addresses list array */
static void add_to_array(uint16_t val)
{
    ScreenAddresses[AddressIndex++] = val;
}

/* Add another "attributes" address to the screen addresses list array */
static void add_colour(uint16_t addr, uint8_t IYh)
{ // COLOUR_LOAD
    if (!parity(IYh))
        addr ^= 0xffff;
    if ((addr & 0x700) == 0)
        add_to_array(((addr >> 11) & 0x300) | 0x5800);
}

/* Here we get the address in screen memory representing one pixel down */
/* or up from addr, depending on the parity of the IYh variable */
static uint16_t next_line_addr(uint16_t addr, uint8_t IYh)
{
    // pixel down
    if (parity(IYh)) {
        addr += 0x100;
        if ((addr & 0x700) != 0) // 0000 0111 0000 0000
            return addr;

        addr += 0x20;
        if ((addr & 0xe0) == 0) // 1110 0000 {
            return addr - 0x100;

        return addr - 0x800;
    }
    // pixel up
    addr -= 0x100;
    if ((addr ^ 0xffff) & 0x700)
        return addr;
    addr -= 0x20;
    if ((addr ^ 0xffff) & 0xe0)
        return addr + 0x800;
    return addr + 0x100;
}

static void push_registers_on_stack(uint8_t ack, uint16_t IY, uint16_t BC, uint16_t DE, uint16_t HL, uint8_t D)
{
    uint16_t SP = 0xee57 + D * 11;
    // D can be 1, 2 or 3, selecting a different "slot" at 0xee62, 0xee6d or 0xee78 respectively
    SP = push(IY, SP);
    SP = push(BC, SP);
    SP = push(DE, SP);
    SP = push(HL, SP);
    uint16_t AF = ack * 256;
    push(AF, SP);
}

/* Here we determine in which direction to go for the next screen byte. */
/* Returns 0 of 0xff */
static uint8_t styler(uint8_t ack, uint16_t *IY, uint16_t *HL, uint16_t *DE, uint16_t *BC)
{ // DIRECTION_OF_LOAD

    if (ack < 1 || ack > 3) {
        fprintf(stderr, "Error! Bad ackumulator value! (%d)\n", ack);
        exit(1);
    }

    uint16_t SP = 0xee4d + ack * 11;
    // ack can be 1, 2 or 3, selecting a different "slot" at 0xee58, 0xee63 or 0xee6e, respectively
    uint16_t AF = pop(&SP);
    uint8_t L;
    *HL = pop(&SP);
    *DE = pop(&SP);
    *BC = pop(&SP);
    *IY = pop(&SP);
    if ((AF >> 8) == 0)
        return 0;
    uint8_t IYh = *IY >> 8;

    add_colour(*HL, IYh);
    add_to_array(*HL);
    *BC -= 0x100;

    if (IYh < 2) {
        if ((*BC >> 8) != 0) {
            *HL = next_line_addr(*HL, IYh); // find address of next line
            return 0xff;
        }
        L = *BC & 0xff;
        if (!parity(IYh)) {
            L--;
        } else {
            L++;
        }
        *HL = (*DE & 0xff00) | L;
        *BC = (*IY << 8) | L;
        *DE = (*DE & 0xff) - 1;
        if (*DE == 0)
            return 0;
        return 0xff;
    }
    // STYLE34
    L = *HL & 0xff;
    if (!parity(IYh)) {
        L--;
    } else {
        L++;
    }
    *HL = (*HL & 0xff00) | L;

    if ((*BC >> 8) != 0) {
        return 0xff;
    }
    *HL = next_line_addr((*HL & 0xff00) | (*BC & 0xff), IYh); // find address of next line
    *BC = (*DE << 8) | (*HL & 0xff);
    *DE -= 0x100;
    if ((*DE >> 8) == 0)
        return 0;
    return 0xff;
}

static void address_table(void)
{
    uint16_t IY = 0x032d;
    uint16_t IX = 0xeeab;
    uint16_t DE, BC, HL;
    uint16_t counter = mem[0xeea9] | mem[0xeeaa] * 0x100;
    uint8_t ack, B, C;
    do { // next line
        ack = mem[IX + 3] >> 5;
        C = ack + 1;
        for (int i = C; i > 0; i--) {
            HL = mem[IX] | mem[IX + 1] * 0x100;
            ack = mem[IX + 3] & 0x1f; // 0001 1111
            DE = (mem[IX + 2] & 0x3f) + ack * 0x800; // 0011 1111
            ack = mem[IX + 2] >> 6;
            IY = ack * 0x100 | (IY & 0xff);
            BC = (DE << 8) | (HL & 0xff);
            if (ack < 2) {
                IY = ack * 0x100 | (DE >> 8);
                BC = (DE >> 8) * 0x100 | mem[IX];
                DE = mem[IX + 1] * 0x100 | (DE & 0xff);
            }
            ack++;
            push_registers_on_stack(ack, IY, BC, DE, HL, i);
            IX += 4;
            counter--;
        }
        do { // NXTP1
            B = 0;
            for (int i = C; i > 0; i--) {
                // styler() returns 0 or 0xff
                ack = styler(i, &IY, &HL, &DE, &BC);
                push_registers_on_stack(ack, IY, BC, DE, HL, i);
                B += ack;
            }
        } while (B != 0);
    } while (counter != 0);
}

uint8_t *LoadAlkatrazPicture(uint8_t *memimage, uint8_t *file)
{
    mem = memimage;
    ScreenAddresses = MemAlloc(SCREEN_MEMORY_SIZE * sizeof(uint16_t));
    address_table();
    uint8_t loacon = 0xf6;
    DeAlkatraz(file, mem, 0, 0xea7d, 2, &loacon, 0xc1, 0xcb, 1);
    uint16_t fileoffset = 2;
    for (int i = 0; i < SCREEN_MEMORY_SIZE; i++)
        DeAlkatraz(file, mem, fileoffset++, ScreenAddresses[i], 1, &loacon, 0xc1, 0xcb, 1);
    free(ScreenAddresses);
    return mem;
}

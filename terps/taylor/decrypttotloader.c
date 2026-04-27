//
//  decryptloader.c
//
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Used for de-protecting the ZX Spectrum text-only version of Temple of Terror.
//
//  The Temple of Terror tape image uses the "Alkatraz" copy protection scheme,
//  which encrypts game data behind layers of self-modifying Z80 machine code.
//  Each decryption pass reveals a small Z80 routine that performs the next pass,
//  repeating dozens of times before the actual game data is exposed.
//
//  This file re-implements those Z80 decryption routines in C. The functions
//  below are named after the Z80 instructions or registers they emulate
//  (HL, BC, DE, R, SP, etc.), preserving the structure of the original
//  protection code.
//
//  Created by Petter Sjölund on 2022-04-18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extracttape.h"
#include "loadtotpicture.h"
#include "utility.h"

#include "decrypttotloader.h"

// Adds a constant delta to each byte in the range [HL, HL+BC).
// Emulates: ADD (HL), delta / INC HL / DJNZ
static void loop_with_delta(uint8_t *mem, uint16_t HL, uint16_t BC, int8_t delta)
{
    for (int i = 0; i < BC; i++) {
        mem[HL++] += delta;
    }
}

// XORs each byte with an accumulator (D) that chains across iterations,
// creating a CBC-like (cipher block chaining) dependency between bytes.
// The direction parameter controls whether we traverse forward (+1) or backward (-1).
// Emulates: LD A,(HL) / XOR D / LD (HL),A / LD D,A
static void loop_with_d(uint8_t *mem, uint8_t D, uint16_t HL, uint16_t BC, int direction)
{
    for (int i = 0; i < BC; i++) {
        D = mem[HL] ^ D;
        mem[HL] = D;
        HL += direction;
    }
}

// XORs each byte with a pseudo-random value derived from the Z80 R (refresh)
// register. R increments by 9 each iteration and wraps at 0x80, simulating
// the Z80's 7-bit refresh counter behavior.
static void loop_with_r(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        mem[HL] = R ^ mem[HL];
        HL++;
        R += 0x9;
        R %= 0x80;
    }
}

// XORs each byte with both the high and low bytes of its own address.
// Uses the memory address itself as a position-dependent key.
static void loop_with_xor_hl(uint8_t *mem, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        mem[HL] = mem[HL] ^ (HL >> 8);
        mem[HL] = mem[HL] ^ (HL & 0xff);
        HL++;
    }
}

// Negates (two's complement) each byte in the range.
// Emulates: NEG applied to each byte via LD A,(HL) / NEG / LD (HL),A
static void loop_with_neg(uint8_t *mem, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        mem[HL] = -mem[HL];
        HL++;
    }
}

// Bitwise complements (flips all bits of) each byte in the range.
// Emulates: CPL (complement accumulator), equivalent to XOR 0xFF.
static void loop_with_cpl(uint8_t *mem, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        mem[HL] = mem[HL] ^ 0xff;
        HL++;
    }
}

// Adds or subtracts a constant value from each byte, with an initial carry
// applied only to the first byte. The sign parameter (+1 or -1) determines
// whether the carry is added or subtracted. Direction controls traversal order.
// Emulates: ADD A,val / SCF or SUB val / SCF with carry flag set on first byte.
static void loop_with_sub_or_add(uint8_t *mem, uint16_t HL, uint16_t BC, int8_t val, int sign, int direction)
{
    int carry = 1;
    for (int i = 0; i < BC; i++) {
        mem[HL] = mem[HL] + val + carry * sign;
        carry = 0;
        HL += direction;
    }
}

// Rotates each byte left by one bit, wrapping the top bit into the bottom.
// Emulates: RLC (HL) — rotate left circular.
static void loop_with_rotate_left(uint8_t *mem, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        int carry = rotate_left_with_carry(&mem[HL], 0);
        mem[HL] |= carry;
        HL++;
    }
}

// Rotates each byte right by one bit, wrapping the bottom bit into the top.
// Emulates: RRC (HL) — rotate right circular.
static void loop_with_rotate_right(uint8_t *mem, uint16_t HL, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        int carry = rotate_right_with_carry(&mem[HL], 0);
        mem[HL] |= (carry * 128);
        HL++;
    }
}

// XORs each byte with the R register, both bytes of DE, and both bytes of
// the loop counter. DE starts at 0x61A8 and increments each iteration.
// The combination of R, DE, and the counter creates a complex key stream.
static void loop_with_r_and_de(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t BC)
{
    uint16_t DE = 0x61a8;
    for (uint16_t i = BC; i > 0; i--) {
        uint8_t A = R ^ mem[HL];
        A = A ^ (DE & 0xff);
        A = A ^ (DE >> 8);
        A = A ^ (i & 0xff);
        A = A ^ (i >> 8);
        DE++;
        mem[HL++] = A;
        R += 0x11;
        R %= 0x80;
    }
}

// Operates on word-aligned (2-byte) pairs starting at SP. For each pair,
// XORs one byte with R, then swaps the bytes within the pair.
// The dfirst flag selects whether the high byte (D) or low byte (E) is
// XORed first, emulating different POP/PUSH register orderings.
// HL here is the iteration count, not an address.
static void loop_with_r_and_sp(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t SP, int dfirst)
{
    uint8_t A, D, E;
    for (int i = 0; i < HL; i++) {
        D = mem[SP + 1];
        E = mem[SP];
        A = dfirst ? D : E;
        A = A ^ R;
        if (dfirst) {
            D = E;
            E = A;
        } else {
            E = D;
            D = A;
        }
        mem[SP] = E;
        mem[SP + 1] = D;
        SP += 2;
        R += 0xe;
        R %= 0x80;
    }
}

// Similar to loop_with_r_and_sp but traverses downward from a fixed address
// (0xEEFF). XORs the low byte of each pair with R, then swaps the pair.
// HL here is the iteration count.
static void loop_with_r_and_sp2(uint8_t *mem, uint8_t R, uint16_t HL)
{
    uint16_t SP = 0xeeff;
    for (int i = 0; i < HL; i++) {
        uint8_t D = mem[SP] ^ R;
        mem[SP] = mem[SP + 1];
        mem[SP + 1] = D;
        SP -= 2;
        R += 0xf;
        R %= 0x80;
    }
}

// Stores a return address at SP, then XORs each byte with R and both bytes
// of a decrementing counter. Named after the Z80 EXX instruction (exchange
// shadow registers) which the original code uses to juggle register sets.
static void loop_with_exx(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t SP, uint16_t DE, uint16_t addr)
{
    mem[SP] = addr & 0xff;
    mem[SP + 1] = addr >> 8;
    for (uint16_t i = DE; i > 0; i--) {
        uint8_t A = R ^ (i >> 8);
        A = A ^ mem[HL];
        mem[HL++] = A ^ (i & 0xff);
        R += 0xf;
        R %= 0x80;
    }
}

// XORs each byte with both R and a fixed byte at address IY.
// IY points to a single key byte that is reused for every iteration.
static void loop_with_r_and_iy(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t BC, uint16_t IY)
{
    for (int i = 0; i < BC; i++) {
        mem[HL] = R ^ mem[HL];
        mem[HL] = mem[HL] ^ mem[IY];
        HL++;
        R += 0xE;
        R %= 0x80;
    }
}

// XORs bytes at an advancing IY pointer with R and both bytes of a
// decrementing HL counter, combining address-based and R-based key streams.
// Unlike most other loops, this writes to IY (not HL), and IY advances
// while HL counts down.
static void loop_with_r_and_iy2(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t BC, uint16_t IY)
{
    for (int i = 0; i < BC; i++) {
        mem[IY] = R ^ mem[IY];
        mem[IY] = mem[IY] ^ (HL >> 8);
        mem[IY] = mem[IY] ^ (HL & 0xff);
        HL--;
        IY++;
        R += 0xf;
        R %= 0x80;
    }
}

// Processes two bytes per iteration of the outer loop. Each byte is XORed
// with R and both bytes of DE. R advances differently on the first vs second
// byte of each pair, and DE increments once per pair, creating a 16-bit
// block-oriented key schedule.
static void double_loop_with_r(uint8_t *mem, uint8_t R, uint16_t HL, uint16_t DE, uint16_t BC)
{
    for (int i = 0; i < BC; i++) {
        for (int j = 0; j < 2; j++) {
            uint8_t A = R ^ (DE & 0xff);
            A = A ^ (DE >> 8);
            mem[HL] = A ^ mem[HL];
            HL++;
            if (j != 1) {
                R += 0x9;
                R %= 0x80;
            }
        }
        DE++;
        R += 0x12;
        R %= 0x80;
    }
}

// The original self-modifying Z80 code would transform itself over and over,
// each time decrypting a small Z80 code snippet that performs the next
// transformation, until finally arriving at the actual loading screen
// drawing code.
//
// We need the resulting code to XOR it with the encrypted game data in order
// to decrypt it, so we have to perform all the equivalent transformations
// here. Each line below corresponds to one decryption pass from the original
// protection, applied in strict sequence to a 64 KB memory image.
static void DecryptToTLoader(uint8_t *mem)
{
    loop_with_d(mem, 0x3c, 0xe142, 0xdbf, 1);
    loop_with_d(mem, 0x39, 0xef00, 0xdac, -1);
    loop_with_r(mem, 0x7, 0xe164, 0xe164);
    loop_with_sub_or_add(mem, 0xe175, 0x0d8c, 0x27, 1, 1);
    loop_with_r_and_de(mem, 0x75, 0xe194, 0x0d6d);
    loop_with_d(mem, 0x2d, 0xe1a6, 0x0d5b, 1);
    loop_with_sub_or_add(mem, 0xe1b7, 0x0d4a, (int8_t)-0x2e, -1, 1);
    // Zero-fill memory from 0xEF01 to 0xFFFF (LDIR block copy of zeroes)
    mem[0xef02] = 0;
    ldir(mem, 0xef02, 0xef01, 0x10fd);
    loop_with_d(mem, 0x2b, 0xe1d7, 0x0d2a, 1);
    loop_with_xor_hl(mem, 0xe1e7, 0x0d1a);
    loop_with_r_and_de(mem, 0x26, 0xe206, 0x0cfb);
    double_loop_with_r(mem, 0x53, 0xe224, 0x0013, 0x066e);
    loop_with_r(mem, 0x6e, 0xe235, 0x0ccc);
    loop_with_sub_or_add(mem, 0xe246, 0x0cbb, (int8_t)-0x18, -1, 1);
    // Zero-fill ZX Spectrum screen memory and beyond (0x4000–0xE253)
    mem[0x4000] = 0;
    ldir(mem, 0x4000 + 1, 0x4000, 0xa253);
    loop_with_d(mem, 5, 0xe266, 0x0c9b, 1);
    loop_with_r_and_de(mem, 0x1c, 0xe285, 0x0c7c);
    loop_with_r(mem, 0x57, 0xe295, 0x0c6c);
    loop_with_d(mem, 0xc, 0xe2a7, 0x0c5a, 1);
    loop_with_r_and_sp(mem, 0x52, 0x0622, 0xe2bc, 1);
    loop_with_sub_or_add(mem, 0xe2ce, 0x0c33, 0xa, 1, 1);
    loop_with_r(mem, 0x4b, 0xe2de, 0x0c23);
    loop_with_r_and_de(mem, 0x0c, 0xe2fd, 0x0c04);
    double_loop_with_r(mem, 0x52, 0xe31b, 0x0372, 0x05f2);
    loop_with_r_and_sp(mem, 0x59, 0x05e7, 0xe331, 0);
    loop_with_r(mem, 0x7d, 0xe342, 0x0bbf);
    loop_with_d(mem, 0x64, 0xef00, 0x0bac, -1);
    loop_with_r(mem, 0x46, 0xe366, 0x0b9b);
    loop_with_delta(mem, 0xe373, 0x0b8e, -1);
    loop_with_sub_or_add(mem, 0xe373, 0x0b8e, 0x6f, 1, 1);
    loop_with_d(mem, 0x68, 0xe396, 0x0b6b, 1);
    loop_with_r_and_sp(mem, 0x42, 0x05aa, 0xe3ab, 1);
    loop_with_r(mem, 0x10, 0xe3bc, 0x0b45);
    double_loop_with_r(mem, 0x02, 0xe3da, 0x0150, 0x0593);
    loop_with_d(mem, 0x5c, 0xe3ed, 0x0b14, 1);
    loop_with_neg(mem, 0xe3fd, 0x0b04);
    loop_with_xor_hl(mem, 0xe40d, 0x0af4);
    loop_with_r_and_sp(mem, 0x77, 0x056f, 0xe422, 0);
    loop_with_d(mem, 0x40, 0xef00, 0x0acb, -1);
    loop_with_sub_or_add(mem, 0xe446, 0x0abb, 0x4d, 1, 1);
    loop_with_r(mem, 0x0d, 0xe458, 0x0aa9);
    loop_with_cpl(mem, 0xe467, 0x0a9a);
    loop_with_sub_or_add(mem, 0xef00, 0x0a88, 0xb7, 1, -1);
    loop_with_rotate_left(mem, 0xe486, 0x0a7b);
    loop_with_sub_or_add(mem, 0xef00, 0x0a69, 0xb1, 1, -1);
    loop_with_d(mem, 0xbc, 0xe4a9, 0x0a58, 1);
    loop_with_delta(mem, 0xe4b6, 0x0a4b, 1);
    loop_with_sub_or_add(mem, 0xe4c7, 0x0a3a, 0xa6, 1, 1);
    loop_with_d(mem, 0xa5, 0xef00, 0x0a27, -1);
    loop_with_r_and_sp(mem, 0x53, 0x0509, 0xe4ee, 1);
    loop_with_r(mem, 0x53, 0xe4ff, 0x0a02);
    loop_with_r_and_sp(mem, 0x67, 0x04f6, 0xe514, 0);
    loop_with_r(mem, 0x5d, 0xe525, 0x09dc);
    loop_with_d(mem, 0x94, 0xef00, 0x09c9, -1);
    loop_with_r_and_iy(mem, 0x31, 0xe552, 0x09af, 0xe547);
    loop_with_sub_or_add(mem, 0xef00, 0x099d, (int8_t)-0x9e, -1, -1);
    double_loop_with_r(mem, 0x31, 0xe581, 0x0f9c, 0x04bf);
    loop_with_r(mem, 0x58, 0xe594, 0xe594);
    loop_with_r_and_sp2(mem, 0x2f, 0x04aa);
    loop_with_r_and_iy2(mem, 0x29, 0x0485, 0x093a, 0xe5c7);
    loop_with_xor_hl(mem, 0xe5d7, 0x092a);
    loop_with_exx(mem, 0x16, 0xe5fb, 0xe5dc, 0x0906, 0xe5ed);
    loop_with_rotate_left(mem, 0xe609, 0x08f8);
    loop_with_xor_hl(mem, 0xe619, 0x08e8);
    loop_with_rotate_right(mem, 0xe627, 0x08da);
    loop_with_neg(mem, 0xe637, 0x08ca);
    loop_with_d(mem, 0xff, 0xef00, 0x08b7, -1);
    loop_with_r(mem, 0x6c, 0xe659, 0x08a8);
    loop_with_rotate_left(mem, 0xe667, 0x089a);
    loop_with_neg(mem, 0xe677, 0x088a);
    loop_with_d(mem, 0xe3, 0xef00, 0x0877, -1);
    loop_with_neg(mem, 0xe699, 0x0868);
    loop_with_r_and_sp2(mem, 0x46, 0x0428);
    loop_with_d(mem, 0xe8, 0xef00, 0x083e, -1);
    loop_with_sub_or_add(mem, 0xe6d3, 0x082e, 0xd7, 1, 1);
    loop_with_r_and_sp(mem, 0x44, 0x040c, 0xe6e8, 0);
    loop_with_rotate_right(mem, 0xe6f7, 0x080a);
    loop_with_delta(mem, 0xe704, 0x07fd, 1);
    loop_with_r_and_sp(mem, 0x26, 0x03f3, 0xe719, 1);
    loop_with_delta(mem, 0xe72a, 0x07d7, 0xc6);
    loop_with_sub_or_add(mem, 0xef00, 0x07c5, (int8_t)-0xc5, -1, -1);
    loop_with_d(mem, 0xc0, 0xef00, 0x07b3, -1);
    loop_with_delta(mem, 0xe75a, 0x07a7, 1);
    loop_with_rotate_right(mem, 0xe768, 0x0799);
    loop_with_r(mem, 0x42, 0xe778, 0x0789);
    loop_with_d(mem, 0x34, 0xe78a, 0x0777, 1);
    loop_with_rotate_right(mem, 0xe798, 0x0769);
    loop_with_r(mem, 0x28, 0xe7a8, 0x0759);
    loop_with_sub_or_add(mem, 0xef00, 0x0747, 0x3d, 1, -1);
    loop_with_d(mem, 0x38, 0xe7cb, 0x0736, 1);
    loop_with_cpl(mem, 0xe7da, 0x0727);
    loop_with_r(mem, 0x2a, 0xe7ec, 0x0715);
    loop_with_neg(mem, 0xe7fc, 0x0705);
    loop_with_rotate_left(mem, 0xe80a, 0x06f7);
    loop_with_r(mem, 0x5b, 0xe81a, 0x06e7);
    loop_with_neg(mem, 0xe82a, 0x06d7);
    loop_with_rotate_left(mem, 0xe838, 0x06c9);
    loop_with_r_and_sp(mem, 0xE, 0x0359, 0xe84d, 0);
    loop_with_exx(mem, 0x77, 0xe872, 0xe853, 0x068f, 0xe864);
    loop_with_xor_hl(mem, 0xe882, 0x067f);
    loop_with_rotate_left(mem, 0xe890, 0x0671);
    loop_with_r(mem, 0x6c, 0xe8a0, 0x0661);
    loop_with_neg(mem, 0xe8b0, 0x0651);
    loop_with_d(mem, 0x0e, 0xe8c2, 0x063f, 1);
    loop_with_sub_or_add(mem, 0xef00, 0x062d, (int8_t)-0x0d, -1, -1);
    loop_with_d(mem, 0x08, 0xef00, 0x061b, -1);
    loop_with_sub_or_add(mem, 0xe8f6, 0x060b, (int8_t)-0x77, -1, 1);
    loop_with_xor_hl(mem, 0xe906, 0x05fb);
    loop_with_r_and_de(mem, 1, 0xe925, 0x05dc);
    loop_with_neg(mem, 0xe935, 0x05cc);
    loop_with_d(mem, 0x7b, 0xef00, 0x05b9, -1);
    loop_with_exx(mem, 0x57, 0xe96b, 0xe94c, 0x0596, 0xe95d);
    loop_with_d(mem, 0x65, 0xe97d, 0x0584, 1);
    loop_with_r_and_iy2(mem, 0x4c, 0x0263, 0x0568, 0xe999);
    loop_with_r(mem, 0x66, 0xe9a9, 0x0558);
    loop_with_exx(mem, 0x09, 0xe9cd, 0xe9ae, 0x0534, 0xe9bf);
    loop_with_sub_or_add(mem, 0xe9de, 0x0523, 0x69, 1, 1);
    loop_with_sub_or_add(mem, 0xe9ef, 0x0512, (int8_t)-0x54, -1, 1);
    loop_with_d(mem, 0x53, 0xea01, 0x0500, 1);
    loop_with_sub_or_add(mem, 0xef00, 0x04ee, (int8_t)-0x5e, -1, -1);
    loop_with_r(mem, 0x3b, 0xea22, 0x04df);
    loop_with_r_and_iy(mem, 0x16, 0xea3d, 0x04c4, 0xea32);
    loop_with_d(mem, 0x47, 0xea4f, 0x04b2, 1);
    loop_with_r(mem, 0x13, 0xea5f, 0x04a2);
}

// XOR the memory at target_address with data from source_address, repeating
// the source cyclically until the entire target region is covered.
//
// This is the final decryption step of the Alkatraz scheme: the encrypted
// game data is XORed with known data (the loading screen bitmap and/or the
// drawing code) to produce the plaintext game image.
void XORAlkatraz(uint8_t *memory, uint16_t target_address, uint16_t source_address, uint16_t source_size, uint16_t target_size)
{
    uint16_t source_start_address = source_address;
    do {
        for (int i = 0; i < source_size; i++) {
            memory[target_address] ^= memory[source_address];
            source_address++;
            target_address++;
            target_size--;
            if (target_size == 0)
                return;
        }
        source_address = source_start_address;
    } while (target_size > 0);
}

// Main entry point: decrypts Temple of Terror Side B from a TZX tape image.
//
// The decryption proceeds in stages:
//   1. Extract and decrypt the tape loader code (Alkatraz-encrypted)
//   2. Run all self-modifying decryption passes to produce the screen drawing code
//   3. Load and decrypt the loading screen image
//   4. XOR the game data with the loading screen and drawing code to recover it
//
// Returns a 64 KB memory buffer containing the decrypted ZX Spectrum memory
// snapshot, or NULL on failure. The caller is responsible for freeing the result.
uint8_t *DecryptToTSideB(uint8_t *data, size_t *original_data_length)
{
    size_t length = *original_data_length;
    // Stage 1: Extract the raw, encrypted tape loader data from the TZX image
    uint8_t *encrypted_loader = GetTZXBlock(3, data, &length);
    if (encrypted_loader == NULL)
        return NULL;
    // Decrypt in two passes, mirroring the byte-for-byte decryption that
    // the original tape loader performed during loading from cassette.
    // Passing NULL as the target makes DeAlkatraz allocate a fresh 64 KB buffer.
    uint8_t loacon = 0xd3;
    uint8_t *memory = DeAlkatraz(encrypted_loader, NULL, 0, 0xdf98, 0x0002, &loacon, 0xde, 0xe8, 1);
    if (memory == NULL) {
        free(encrypted_loader);
        return NULL;
    }
    DeAlkatraz(encrypted_loader, memory, 2, 0xdf9d, 0x0f64, &loacon, 0xde, 0xe8, 1);
    free(encrypted_loader);

    // Initialize key bytes and relocate the loader code via LDIR block copy
    memory[0xef00] = 0x9a;
    memory[0xdf9a] = 0x0f;
    memory[0xdf9d] = 0;
    ldir(memory, 0xdf9e, 0xdf9d, 0x10e);

    // Stage 2: Run all ~90 decryption passes to produce the loading screen
    // drawing code. Although we don't execute this Z80 code directly (there
    // is a C re-implementation in loadtotpicture.c), the resulting byte
    // pattern is needed as an XOR key to decrypt the game data.
    DecryptToTLoader(memory);

    // Stage 3: Load, decrypt and unscramble the loading screen image
    length = *original_data_length;
    uint8_t *block = GetTZXBlock(4, data, &length);
    if (block == NULL) {
        free(memory);
        return NULL;
    }

    // Draw the loading screen into the ZX Spectrum display file (0x4000-0x57FF)
    LoadAlkatrazPicture(memory, block);

    // Decrypt the remaining data blocks from the tape image into memory.
    // These multiple DeAlkatraz passes place the game data across the
    // 64 KB address space at various target addresses.
    loacon = 0x8c;
    DeAlkatraz(block, memory, 0x1b02, 0xea69, 0x000e, &loacon, 0xc1, 0xcb, 1);
    DeAlkatraz(block, memory, 0x1b10, 0x6072, 0x6e96, &loacon, 0xc1, 0xcb, 1);
    DeAlkatraz(block, memory, 0x89a6, 0x5b00, 0x01cc, &loacon, 0xc1, 0xcb, 1);
    DeAlkatraz(block, memory, 0x8b72, 0xea79, 0x0003, &loacon, 0xc1, 0xcb, 1);

    // Clear various control bytes used by the loader
    memory[0xeb0d] = 0;
    memory[0xeb0e] = 0;
    memory[0xeb5e] = 0;
    memory[0xeb5f] = 0;

    memory[0xeb74] = 0;
    memory[0xeaf3] = 0;
    memory[0xeb00] = 0;

    // Stage 4: Final decryption — XOR the game data at 0x8782 with:
    //   (a) the loading screen bitmap at 0x4000 (6 KB, cycled over 18 KB), then
    //   (b) the loading screen drawing code at 0xEA7F (268 bytes, cycled).
    // This peels off the last encryption layer, revealing the actual game.
    XORAlkatraz(memory, 0x8782, 0x4000, 0x1800, 0x4786);
    XORAlkatraz(memory, 0x8782, 0xea7f, 0x010c, 0x4786);

    // Clear the drawing code area now that it's no longer needed as a key
    memory[0xea7f] = 0;
    ldir(memory, 0xea80, 0xea7f, 0x237);

    free(block);
    free(data);

    *original_data_length = 0x10000;

    // Return the 64 KB ZX Spectrum memory snapshot containing the
    // decrypted game data.
    return memory;
}

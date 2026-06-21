/* level9.c  Treaty of Babel module for Level 9 files
 * 2006 By L. Ross Raszewski
 *
 * Note that this module will handle bare Level 9 A-Code, Spectrum .SNA
 * snapshots, and Spectrum .TZX/.Z80 images (the latter two recognised by
 * whole-file (length, 16-bit sum) entries in l9_registry).
 *
 * The Level 9 identification algorithm is based in part on the algorithm
 * used by Paul David Doherty's l9cut program.
 *
 * This file depends on treaty_builder.h
 *
 * This file is public domain, but note that any changes to this file
 * may render it noncompliant with the Treaty of Babel
 */

#define FORMAT level9
#define HOME_PAGE "http://www.if-legends.org/~l9memorial/html/home.html"
#define FORMAT_EXT ".l9,.sna,.dat,.tap,.tzx,.z80,.dsk"
#define NO_METADATA
#define NO_COVER

#include "treaty_builder.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Scanner-derived entries (l9_registry): the v1/v2/v3 recognisers locate the
 * embedded a-code, then look it up by `length` (the a-code length) and `chk`
 * (the a-code's 8-bit byte sum). */
struct l9rec {
    int32 length;
    unsigned char chk;
    char *ifid;
};

/* Whole-file entries (l9_file_registry): tape/snapshot/disk containers (TZX,
 * Z80, CPC disk images…) the a-code recognisers can't see through, identified
 * by the whole file's `length` and its 16-bit byte sum `file_chk`. */
struct l9filerec {
    int32 length;
    uint16_t file_chk;
    char *ifid;
};


static struct l9rec l9_registry[] = {
    { 0x3a31, 0xe5, "LEVEL9-001-1" }, // Adrian Mole I, pt. 1 (BBC)
    { 0x8333, 0xb7, "LEVEL9-001-1" }, // Adrian Mole I, pt. 1 (Commodore 64)
    { 0x7c6f, 0x0f, "LEVEL9-001-1" }, // Adrian Mole I, pt. 1 (Amstrad CPC)
    { 0x72fa, 0x8b, "LEVEL9-001-1" }, // Adrian Mole I, pt. 1 (Spectrum)
    { 0x8324, 0x87, "LEVEL9-001-1" }, // Adrian Mole I, pt. 1 (MSX)
    { 0x38dd, 0x31, "LEVEL9-001-A" }, // Adrian Mole I, pt. 10 (BBC)
    { 0x39c0, 0x44, "LEVEL9-001-B" }, // Adrian Mole I, pt. 11 (BBC)
    { 0x3a12, 0x8f, "LEVEL9-001-C" }, // Adrian Mole I, pt. 12 (BBC)
    { 0x37f1, 0x77, "LEVEL9-001-2" }, // Adrian Mole I, pt. 2 (BBC)
    { 0x844d, 0x50, "LEVEL9-001-2" }, // Adrian Mole I, pt. 2 (Commodore 64)
    { 0x738e, 0x5b, "LEVEL9-001-2" }, // Adrian Mole I, pt. 2 (Spectrum)
    { 0x8442, 0x0f, "LEVEL9-001-2" }, // Adrian Mole I, pt. 2 (MSX)
    { 0x3900, 0x1c, "LEVEL9-001-3" }, // Adrian Mole I, pt. 3 (BBC)
    { 0x8251, 0x5f, "LEVEL9-001-3" }, // Adrian Mole I, pt. 3 (Commodore 64)
    { 0x7375, 0xe5, "LEVEL9-001-3" }, // Adrian Mole I, pt. 3 (Spectrum)
    { 0x823e, 0x5c, "LEVEL9-001-3" }, // Adrian Mole I, pt. 3 (MSX)
    { 0x824e, 0x77, "LEVEL9-001-3" }, // Adrian Mole I, pt. 3 (MSX *reconstructed*)
    { 0x3910, 0xac, "LEVEL9-001-4" }, // Adrian Mole I, pt. 4 (BBC)
    { 0x7a78, 0x5e, "LEVEL9-001-4" }, // Adrian Mole I, pt. 4 (Commodore 64)
    { 0x78d5, 0xe3, "LEVEL9-001-4" }, // Adrian Mole I, pt. 4 (Spectrum)
    { 0x7a71, 0xfb, "LEVEL9-001-4" }, // Adrian Mole I, pt. 4 (MSX)
    { 0x7a75, 0xe3, "LEVEL9-001-4" }, // Adrian Mole I, pt. 4 (MSX *reconstructed*)
    { 0x3ad6, 0xa7, "LEVEL9-001-5" }, // Adrian Mole I, pt. 5 (BBC)
    { 0x38a5, 0x0f, "LEVEL9-001-6" }, // Adrian Mole I, pt. 6 (BBC)
    { 0x361e, 0x7e, "LEVEL9-001-7" }, // Adrian Mole I, pt. 7 (BBC)
    { 0x3934, 0x75, "LEVEL9-001-8" }, // Adrian Mole I, pt. 8 (BBC)
    { 0x3511, 0xcc, "LEVEL9-001-9" }, // Adrian Mole I, pt. 9 (BBC)
    { 0x593a, 0xaf, "LEVEL9-002-1" }, // Adrian Mole II, pt. 1 (BBC)
    { 0x7931, 0xb9, "LEVEL9-002-1" }, // Adrian Mole II, pt. 1 (Commodore 64/Amstrad CPC/MSX)
    { 0x6841, 0x4a, "LEVEL9-002-1" }, // Adrian Mole II, pt. 1 (Spectrum)
    { 0x57e6, 0x8a, "LEVEL9-002-2" }, // Adrian Mole II, pt. 2 (BBC)
    { 0x7cdf, 0xa5, "LEVEL9-002-2" }, // Adrian Mole II, pt. 2 (Commodore 64/Amstrad CPC/MSX)
    { 0x6bc0, 0x62, "LEVEL9-002-2" }, // Adrian Mole II, pt. 2 (Spectrum)
    { 0x5819, 0xcd, "LEVEL9-002-3" }, // Adrian Mole II, pt. 3 (BBC)
    { 0x7a0c, 0x97, "LEVEL9-002-3" }, // Adrian Mole II, pt. 3 (Commodore 64/Amstrad CPC/MSX)
    { 0x692c, 0x21, "LEVEL9-002-3" }, // Adrian Mole II, pt. 3 (Spectrum)
    { 0x579b, 0xad, "LEVEL9-002-4" }, // Adrian Mole II, pt. 4 (BBC)
    { 0x7883, 0xe2, "LEVEL9-002-4" }, // Adrian Mole II, pt. 4 (Commodore 64/Amstrad CPC/MSX)
    { 0x670a, 0x94, "LEVEL9-002-4" }, // Adrian Mole II, pt. 4 (Spectrum)
    { 0x5323, 0xb7, "LEVEL9-003" }, // Adventure Quest (Amstrad CPC/Spectrum)
    { 0x6e60, 0x83, "LEVEL9-003" }, // Adventure Quest /JoD (Amiga/PC)
    { 0x5b58, 0x50, "LEVEL9-003" }, // Adventure Quest /JoD (Atari)
    { 0x63b6, 0x2e, "LEVEL9-003" }, // Adventure Quest /JoD (Commodore 64)
    { 0x6968, 0x32, "LEVEL9-003" }, // Adventure Quest /JoD (Amstrad CPC128/Spectrum +3)
    { 0x5b50, 0x66, "LEVEL9-003" }, // Adventure Quest /JoD (Amstrad CPC64)
    { 0x6970, 0xd6, "LEVEL9-003" }, // Adventure Quest /JoD (Spectrum 128)
    { 0x5ace, 0x11, "LEVEL9-003" }, // Adventure Quest /JoD (Spectrum 48)
    { 0x6e5c, 0xf6, "LEVEL9-003" }, // Adventure Quest /JoD (ST)
    { 0x5323, 0x11, "LEVEL9-003" }, // Adventure Quest (MSX)
    { 0x6972, 0x55, "LEVEL9-003" }, // Adventure Quest /JoD (MSX)
    { 0x1929, 0x00, "LEVEL9-004-DEMO" }, // Champion of the Raj (demo), 1/2 GD (ST)
    { 0x40e0, 0x02, "LEVEL9-004-DEMO" }, // Champion of the Raj (demo), 2/2 GD (ST)
    { 0x3ebb, 0x00, "LEVEL9-004-en" }, // Champion of the Raj (English) 1/2 GD (Amiga)
    { 0x3e4f, 0x00, "LEVEL9-004-en" }, // Champion of the Raj (English) 1/2 GD (PC)
    { 0x3e8f, 0x00, "LEVEL9-004-en" }, // Champion of the Raj (English) 1/2 GD (ST)
    { 0x0fd8, 0x00, "LEVEL9-004-en" }, // Champion of the Raj (English) 2/2 GD (Amiga)
    { 0x14a3, 0x00, "LEVEL9-004-en" }, // Champion of the Raj (English) 2/2 GD (PC)
    { 0x110f, 0x00, "LEVEL9-004-fr" }, // Champion of the Raj (French) 2/2 GD (ST)
    { 0x4872, 0x00, "LEVEL9-004-de" }, // Champion of the Raj (German) 1/2 GD (Amiga)
    { 0x4846, 0x00, "LEVEL9-004-de" }, // Champion of the Raj (German) 1/2 GD (ST)
    { 0x11f5, 0x00, "LEVEL9-004-de" }, // Champion of the Raj (German) 2/2 GD (Amiga / ST)
    { 0x76f4, 0x5e, "LEVEL9-005" }, // Colossal Adventure /JoD (Amiga/PC)
    { 0x5b16, 0x3b, "LEVEL9-005" }, // Colossal Adventure /JoD (Atari)
    { 0x6c8e, 0xb6, "LEVEL9-005" }, // Colossal Adventure /JoD (Commodore 64)
    { 0x6f4d, 0xcb, "LEVEL9-005" }, // Colossal Adventure /JoD (Amstrad CPC128[v1]/Spectrum +3)
    { 0x6f6a, 0xa5, "LEVEL9-005" }, // Colossal Adventure /JoD (Amstrad CPC128[v2])
    { 0x5e31, 0x7c, "LEVEL9-005" }, // Colossal Adventure /JoD (Amstrad CPC64)
    { 0x6f70, 0x40, "LEVEL9-005" }, // Colossal Adventure /JoD (MSX) // Colossal Adventure /JoD (MSX)
    { 0x6f6e, 0x78, "LEVEL9-005" }, // Colossal Adventure /JoD (Spectrum 128)
    { 0x5a8e, 0xf2, "LEVEL9-005" }, // Colossal Adventure /JoD (Spectrum 48)
    { 0x76f4, 0x5a, "LEVEL9-005" }, // Colossal Adventure /JoD (ST)
    { 0x6155, 0xe8, "LEVEL9-005" }, // Colossal Adventure (MSX[A])
    { 0x616a, 0x90, "LEVEL9-005" }, // Colossal Adventure (MSX[B])
    { 0x630e, 0x8d, "LEVEL9-006" }, // Dungeon Adventure (Amstrad CPC)
    { 0x630e, 0xbe, "LEVEL9-006" }, // Dungeon Adventure (MSX)
    { 0x6f0c, 0x95, "LEVEL9-006" }, // Dungeon Adventure /JoD (Amiga/PC/ST)
    { 0x593a, 0x80, "LEVEL9-006" }, // Dungeon Adventure /JoD (Atari)
    { 0x6bd2, 0x65, "LEVEL9-006" }, // Dungeon Adventure /JoD (Commodore 64)
    { 0x6dc0, 0x63, "LEVEL9-006" }, // Dungeon Adventure /JoD (Amstrad CPC128/Spectrum +3)
    { 0x58a6, 0x24, "LEVEL9-006" }, // Dungeon Adventure /JoD (Amstrad CPC64)
    { 0x6de8, 0x4c, "LEVEL9-006" }, // Dungeon Adventure /JoD (Spectrum 128)
    { 0x58a3, 0x38, "LEVEL9-006" }, // Dungeon Adventure /JoD (Spectrum 48)
    { 0x6dea, 0x50, "LEVEL9-006" }, // Dungeon Adventure /JoD (MSX)
    { 0x63be, 0xd6, "LEVEL9-007" }, // Emerald Isle (Atari/Commodore 64/Amstrad CPC/Spectrum/MSX)
    { 0x378c, 0x8d, "LEVEL9-007" }, // Emerald Isle (BBC)
    { 0x63be, 0x0a, "LEVEL9-007" }, // Emerald Isle (MSX *corrupt*)
    { 0x34b3, 0x20, "LEVEL9-008" }, // Erik the Viking (BBC/Commodore 64)
    { 0x34b3, 0xc7, "LEVEL9-008" }, // Erik the Viking (Amstrad CPC)
    { 0x34b3, 0x53, "LEVEL9-008" }, // Erik the Viking (Spectrum)
    { 0x34b3, 0xa8, "LEVEL9-008" }, // Erik the Viking (MSX *converted*)
    { 0xb1a9, 0x80, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (Amiga/ST)
    { 0x908e, 0x0d, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (Commodore 64 Gfx)
    { 0xad41, 0xa8, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (Commodore 64 TO/MSX)
    { 0xb1aa, 0xad, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (PC)
    { 0x8aab, 0xc0, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (Spectrum 48)
    { 0xb0ec, 0xc2, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (ST[v1])
    { 0xb19e, 0x92, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 (ST[v2])
    { 0x5ff0, 0xf8, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 GD (Amstrad CPC/Spectrum +3)
    { 0x52aa, 0xdf, "LEVEL9-009-1" }, // Gnome Ranger, pt. 1 GD (Spectrum 128)
    { 0xab9d, 0x31, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 (Amiga/ST)
    { 0x8f6f, 0x0a, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 (Commodore 64 Gfx)
    { 0xa735, 0xf7, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 (Commodore 64 TO/MSX)
    { 0xab8b, 0xbf, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 (PC/ST[v2])
    { 0x8ac8, 0x9a, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 (Spectrum 48)
    { 0xaf82, 0x83, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 (ST[v1])
    { 0x6024, 0x01, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 GD (Amstrad CPC/Spectrum +3)
    { 0x6ffa, 0xdb, "LEVEL9-009-2" }, // Gnome Ranger, pt. 2 GD (Spectrum 128)
    { 0xae28, 0x87, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 (Amiga/ST)
    { 0x9060, 0xbb, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 (Commodore 64 Gfx)
    { 0xa9c0, 0x9e, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 (Commodore 64 TO/MSX)
    { 0xae16, 0x81, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 (PC/ST[v2])
    { 0x8a93, 0x4f, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 (Spectrum 48)
    { 0xb3e6, 0xab, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 (Unknown)
    { 0x6036, 0x3d, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 GD (Amstrad CPC/Spectrum +3)
    { 0x723a, 0x69, "LEVEL9-009-3" }, // Gnome Ranger, pt. 3 GD (Spectrum 128)
    { 0xd188, 0x13, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (Amiga)
    { 0x9089, 0xce, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (Commodore 64 Gfx)
    { 0xb770, 0x03, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (Commodore 64 TO)
    { 0xd19b, 0xad, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (PC)
    { 0x8ab7, 0x68, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (Spectrum 48)
    { 0xd183, 0x83, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (ST)
    { 0x5a38, 0xf7, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 GD (Amstrad CPC/Spectrum +3)
    { 0x76a0, 0x3a, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 GD (Spectrum 128)
    { 0xbefa, 0x1e, "LEVEL9-010-1" }, // Ingrid's Back, pt. 1 (MSX)
    { 0xc594, 0x03, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (Amiga)
    { 0x908d, 0x80, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (Commodore 64 Gfx)
    { 0xb741, 0xb6, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (Commodore 64 TO)
    { 0xc5a5, 0xfe, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (PC)
    { 0x8b1e, 0x84, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (Spectrum 48)
    { 0xc58f, 0x65, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (ST)
    { 0x531a, 0xed, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 GD (Amstrad CPC/Spectrum +3)
    { 0x7674, 0x0b, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 GD (Spectrum 128)
    { 0xbd57, 0x14, "LEVEL9-010-2" }, // Ingrid's Back, pt. 2 (MSX)
    { 0xd79f, 0xb5, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (Amiga)
    { 0x909e, 0x9f, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (Commodore 64 Gfx)
    { 0xb791, 0xa1, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (Commodore 64 TO)
    { 0xd7ae, 0x9e, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (PC)
    { 0x8b1c, 0xa8, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (Spectrum 48)
    { 0xd79a, 0x57, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (ST)
    { 0x57e4, 0x19, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 GD (Amstrad CPC/Spectrum +3)
    { 0x765e, 0xba, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 GD (Spectrum 128)
    { 0xbf4b, 0x9f, "LEVEL9-010-3" }, // Ingrid's Back, pt. 3 (MSX)
    { 0xbb93, 0x36, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (Amiga)
    { 0x898a, 0x43, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (Apple ][)
    { 0x8970, 0x6b, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (Commodore 64 Gfx)
    { 0xbb6e, 0xa6, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (PC)
    { 0x86d0, 0xb7, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (Spectrum 48)
    { 0xbb6e, 0xad, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (ST)
    { 0x46ec, 0x64, "LEVEL9-011-1" }, // Knight Orc, pt. 1 GD (Amstrad CPC/Spectrum +3)
    { 0x74e0, 0x92, "LEVEL9-011-1" }, // Knight Orc, pt. 1 GD (Spectrum 128)
    { 0xb92d, 0x00, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (MSX *corrupt*)
    { 0xb92d, 0x25, "LEVEL9-011-1" }, // Knight Orc, pt. 1 (MSX)
    { 0xc58e, 0x4a, "LEVEL9-011-2" }, // Knight Orc, pt. 2 (Amiga/ST)
    { 0x8b9f, 0x61, "LEVEL9-011-2" }, // Knight Orc, pt. 2 (Apple ][)
    { 0x8b90, 0x4e, "LEVEL9-011-2" }, // Knight Orc, pt. 2 (Commodore 64 Gfx)
    { 0xc58e, 0x43, "LEVEL9-011-2" }, // Knight Orc, pt. 2 (PC)
    { 0x8885, 0x22, "LEVEL9-011-2" }, // Knight Orc, pt. 2 (Spectrum 48)
    { 0x6140, 0x18, "LEVEL9-011-2" }, // Knight Orc, pt. 2 GD (Amstrad CPC/Spectrum +3)
    { 0x6dbc, 0x97, "LEVEL9-011-2" }, // Knight Orc, pt. 2 GD (Spectrum 128)
    { 0xb734, 0xbc, "LEVEL9-011-2" }, // Knight Orc, pt. 2 (MSX)
    { 0xcb9a, 0x0f, "LEVEL9-011-3" }, // Knight Orc, pt. 3 (Amiga/ST)
    { 0x8af9, 0x61, "LEVEL9-011-3" }, // Knight Orc, pt. 3 (Apple ][)
    { 0x8aea, 0x4e, "LEVEL9-011-3" }, // Knight Orc, pt. 3 (Commodore 64 Gfx)
    { 0xcb9a, 0x08, "LEVEL9-011-3" }, // Knight Orc, pt. 3 (PC)
    { 0x87e5, 0x0e, "LEVEL9-011-3" }, // Knight Orc, pt. 3 (Spectrum 48)
    { 0x640e, 0xc1, "LEVEL9-011-3" }, // Knight Orc, pt. 3 GD (Amstrad CPC/Spectrum +3)
    { 0x7402, 0x07, "LEVEL9-011-3" }, // Knight Orc, pt. 3 GD (Spectrum 128)
    { 0xba02, 0x5f, "LEVEL9-011-3" }, // Knight Orc, pt. 3 (MSX)
    { 0xbba4, 0x94, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Amiga/PC *USA*)
    { 0xc0cf, 0x4e, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Amiga/PC/ST)
    { 0x8afc, 0x07, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Commodore 64 Gfx *USA*)
    { 0x8feb, 0xba, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Commodore 64 Gfx)
    { 0xb4c9, 0x94, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Commodore 64 TO/MSX)
    { 0xc0bd, 0x57, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Mac)
    { 0x8ade, 0xf2, "LEVEL9-012-1" }, // Lancelot, pt. 1 (Spectrum 48)
    { 0x4fd2, 0x9d, "LEVEL9-012-1" }, // Lancelot, pt. 1 GD (BBC)
    { 0x5c7a, 0x44, "LEVEL9-012-1" }, // Lancelot, pt. 1 GD (Amstrad CPC/Spectrum +3)
    { 0x768c, 0xe8, "LEVEL9-012-1" }, // Lancelot, pt. 1 GD (Spectrum 128)
    { 0xd0c0, 0x56, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Amiga/PC *USA*)
    { 0xd5e9, 0x6a, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Amiga/PC/ST)
    { 0x8aec, 0x13, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Commodore 64 Gfx *USA*)
    { 0x8f6b, 0xfa, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Commodore 64 Gfx)
    { 0xb729, 0x51, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Commodore 64 TO/MSX)
    { 0xd5d7, 0x99, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Mac)
    { 0x8b0e, 0xfb, "LEVEL9-012-2" }, // Lancelot, pt. 2 (Spectrum 48)
    { 0x4dac, 0xa8, "LEVEL9-012-2" }, // Lancelot, pt. 2 GD (BBC)
    { 0x53a2, 0x1e, "LEVEL9-012-2" }, // Lancelot, pt. 2 GD (Amstrad CPC/Spectrum +3)
    { 0x76b0, 0x1d, "LEVEL9-012-2" }, // Lancelot, pt. 2 GD (Spectrum 128)
    { 0xb6ac, 0xc6, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Amiga/PC *USA*)
    { 0xbb8f, 0x1a, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Amiga/PC/ST)
    { 0x8aba, 0x0d, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Commodore 64 Gfx *USA*)
    { 0x8f71, 0x2f, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Commodore 64 Gfx)
    { 0xb702, 0xe4, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Commodore 64 TO/MSX)
    { 0xbb7d, 0x17, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Mac)
    { 0x8ab3, 0xc1, "LEVEL9-012-3" }, // Lancelot, pt. 3 (Spectrum 48)
    { 0x4f96, 0x22, "LEVEL9-012-3" }, // Lancelot, pt. 3 GD (BBC)
    { 0x5914, 0x22, "LEVEL9-012-3" }, // Lancelot, pt. 3 GD (Amstrad CPC/Spectrum +3)
    { 0x765e, 0x4f, "LEVEL9-012-3" }, // Lancelot, pt. 3 GD (Spectrum 128)
    { 0x5eb9, 0x30, "LEVEL9-013" }, // Lords of Time (Amstrad CPC)
    { 0x5eb9, 0x5d, "LEVEL9-013" }, // Lords of Time (MSX *corrupt*)
    { 0x5eb9, 0x6e, "LEVEL9-013" }, // Lords of Time (Spectrum/MSX *fixed*)
    { 0xb257, 0xf8, "LEVEL9-013" }, // Lords of Time /T&M (Amiga *USA*)
    { 0xb576, 0x2a, "LEVEL9-013" }, // Lords of Time /T&M (Amiga)
    { 0x8d78, 0x3a, "LEVEL9-013" }, // Lords of Time /T&M (Commodore 64 Gfx *USA*)
    { 0x9070, 0x43, "LEVEL9-013" }, // Lords of Time /T&M (Commodore 64 Gfx)
    { 0xb38c, 0x37, "LEVEL9-013" }, // Lords of Time /T&M (Commodore 64 TO)
    { 0xb563, 0x6a, "LEVEL9-013" }, // Lords of Time /T&M (Mac)
    { 0xb57c, 0x44, "LEVEL9-013" }, // Lords of Time /T&M (PC)
    { 0xb260, 0xe5, "LEVEL9-013" }, // Lords of Time /T&M (PC/ST *USA*)
    { 0x8950, 0xa1, "LEVEL9-013" }, // Lords of Time /T&M (Spectrum 48)
    { 0xb579, 0x89, "LEVEL9-013" }, // Lords of Time /T&M (ST)
    { 0x579e, 0x97, "LEVEL9-013" }, // Lords of Time /T&M GD (BBC)
    { 0x69fe, 0x56, "LEVEL9-013" }, // Lords of Time /T&M GD (Amstrad CPC/Spectrum +3)
    { 0x6f1e, 0xda, "LEVEL9-013" }, // Lords of Time /T&M GD (Spectrum 128)
    { 0xb393, 0x11, "LEVEL9-013" }, // Lords of Time /T&M (MSX)
    { 0x5671, 0xbc, "LEVEL9-014" }, // Price of Magik (BBC)
    { 0x6fc6, 0x14, "LEVEL9-014" }, // Price of Magik (Commodore 64)
    { 0x5aa4, 0xc1, "LEVEL9-014" }, // Price of Magik (Spectrum 48 / Amstrad CPC)
    { 0x7410, 0x5e, "LEVEL9-014" }, // Price of Magik (Spectrum 128)
    { 0xb797, 0x1f, "LEVEL9-014" }, // Price of Magik /T&M (Amiga *USA*)
    { 0xbaca, 0x3a, "LEVEL9-014" }, // Price of Magik /T&M (Amiga)
    { 0x8c46, 0xf0, "LEVEL9-014" }, // Price of Magik /T&M (Commodore 64 Gfx *USA*)
    { 0x8f51, 0xb2, "LEVEL9-014" }, // Price of Magik /T&M (Commodore 64 Gfx)
    { 0xb451, 0xa8, "LEVEL9-014" }, // Price of Magik /T&M (Commodore 64 TO)
    { 0xbab2, 0x87, "LEVEL9-014" }, // Price of Magik /T&M (Mac)
    { 0xbac7, 0x7f, "LEVEL9-014" }, // Price of Magik /T&M (PC)
    { 0xb7a0, 0x7e, "LEVEL9-014" }, // Price of Magik /T&M (PC/ST *USA*)
    { 0x8a60, 0x2a, "LEVEL9-014" }, // Price of Magik /T&M (Spectrum 48)
    { 0xbac4, 0x80, "LEVEL9-014" }, // Price of Magik /T&M (ST)
    { 0x579a, 0x2a, "LEVEL9-014" }, // Price of Magik /T&M GD (BBC)
    { 0x5a50, 0xa9, "LEVEL9-014" }, // Price of Magik /T&M GD (Amstrad CPC/Spectrum +3)
    { 0x6108, 0xdd, "LEVEL9-014" }, // Price of Magik /T&M GD (Spectrum 128)
    { 0x7334, 0x87, "LEVEL9-014" }, // Price of Magik (MSX)
    { 0xb8b5, 0x27, "LEVEL9-014" }, // Price of Magik /T&M (MSX)
    { 0x506c, 0xf0, "LEVEL9-015" }, // Red Moon (BBC/Commodore 64/Amstrad CPC/MSX)
    { 0x505d, 0x32, "LEVEL9-015" }, // Red Moon (Spectrum)
    { 0xa398, 0x82, "LEVEL9-015" }, // Red Moon /T&M (Amiga *USA*)
    { 0xa692, 0xd1, "LEVEL9-015" }, // Red Moon /T&M (Amiga)
    { 0x8d56, 0xd3, "LEVEL9-015" }, // Red Moon /T&M (Commodore 64 Gfx *USA*)
    { 0x903f, 0x6b, "LEVEL9-015" }, // Red Moon /T&M (Commodore 64 Gfx)
    { 0xa4e2, 0xa6, "LEVEL9-015" }, // Red Moon /T&M (Commodore 64 TO)
    { 0xa67c, 0xb8, "LEVEL9-015" }, // Red Moon /T&M (Mac)
    { 0xa69e, 0x6c, "LEVEL9-015" }, // Red Moon /T&M (PC)
    { 0xa3a4, 0xdf, "LEVEL9-015" }, // Red Moon /T&M (PC/ST *USA*)
    { 0x8813, 0x11, "LEVEL9-015" }, // Red Moon /T&M (Spectrum 48)
    { 0xa698, 0x41, "LEVEL9-015" }, // Red Moon /T&M (ST)
    { 0x5500, 0x50, "LEVEL9-015" }, // Red Moon /T&M GD (BBC)
    { 0x6888, 0x8d, "LEVEL9-015" }, // Red Moon /T&M GD (Amstrad CPC/Spectrum +3)
    { 0x6da0, 0xb8, "LEVEL9-015" }, // Red Moon /T&M GD (Spectrum 128)
    { 0xa4d8, 0xd9, "LEVEL9-015" }, // Red Moon /T&M (MSX)
    { 0x6064, 0xbd, "LEVEL9-016" }, // Return to Eden (Atari *corrupt*)
    { 0x6064, 0x01, "LEVEL9-016" }, // Return to Eden (BBC[v1])
    { 0x6047, 0x6c, "LEVEL9-016" }, // Return to Eden (BBC[v2])
    { 0x6064, 0xda, "LEVEL9-016" }, // Return to Eden (Commodore 64[v2] *corrupt*)
    { 0x6064, 0x95, "LEVEL9-016" }, // Return to Eden (Commodore 64[v2])
    { 0x60c4, 0x28, "LEVEL9-016" }, // Return to Eden (Amstrad CPC/Commodore 64[v1])
    { 0x5cb7, 0xfe, "LEVEL9-016" }, // Return to Eden (MSX)
    { 0x5ca1, 0x33, "LEVEL9-016" }, // Return to Eden (Spectrum[v1])
    { 0x5cb7, 0x64, "LEVEL9-016" }, // Return to Eden (Spectrum[v2])
    { 0x7d16, 0xe6, "LEVEL9-016" }, // Return to Eden /SD (Amiga/ST)
    { 0x639c, 0x8b, "LEVEL9-016" }, // Return to Eden /SD (Apple ][)
    { 0x60f7, 0x68, "LEVEL9-016" }, // Return to Eden /SD (Atari)
    { 0x772f, 0xca, "LEVEL9-016" }, // Return to Eden /SD (Commodore 64/MSX)
    { 0x7cff, 0xf8, "LEVEL9-016" }, // Return to Eden /SD (Amstrad CPC/Spectrum +3)
    { 0x7cf8, 0x24, "LEVEL9-016" }, // Return to Eden /SD (Mac)
    { 0x7d14, 0xe8, "LEVEL9-016" }, // Return to Eden /SD (PC)
    { 0x7c55, 0x18, "LEVEL9-016" }, // Return to Eden /SD (Spectrum 128)
    { 0x5f43, 0xca, "LEVEL9-016" }, // Return to Eden /SD (Spectrum 48)
    { 0xc132, 0x14, "LEVEL9-017-1" }, // Scapeghost, pt. 1 (Amiga *bak*)
    { 0xbeab, 0x2d, "LEVEL9-017-1" }, // Scapeghost, pt. 1 (Amiga)
    { 0x9058, 0xcf, "LEVEL9-017-1" }, // Scapeghost, pt. 1 (Commodore 64 Gfx)
    { 0xbe94, 0xcc, "LEVEL9-017-1" }, // Scapeghost, pt. 1 (PC/ST)
    { 0x8a21, 0xf4, "LEVEL9-017-1" }, // Scapeghost, pt. 1 (Spectrum 48)
    { 0x55ce, 0xa1, "LEVEL9-017-1" }, // Scapeghost, pt. 1 GD (BBC)
    { 0x5cbc, 0xa5, "LEVEL9-017-1" }, // Scapeghost, pt. 1 GD (Amstrad CPC/Spectrum +3)
    { 0x762e, 0x82, "LEVEL9-017-1" }, // Scapeghost, pt. 1 GD (Spectrum 128)
    { 0xb613, 0x4c, "LEVEL9-017-1" }, // Scapeghost, pt. 1 (Commodore 64 TO/MSX *converted*)
    { 0x99bd, 0x65, "LEVEL9-017-2" }, // Scapeghost, pt. 2 (Amiga/PC/ST)
    { 0x8f43, 0xc9, "LEVEL9-017-2" }, // Scapeghost, pt. 2 (Commodore 64 Gfx)
    { 0x8a12, 0xe3, "LEVEL9-017-2" }, // Scapeghost, pt. 2 (Spectrum 48)
    { 0x54a6, 0xa9, "LEVEL9-017-2" }, // Scapeghost, pt. 2 GD (BBC)
    { 0x5932, 0x4e, "LEVEL9-017-2" }, // Scapeghost, pt. 2 GD (Amstrad CPC/Spectrum +3)
    { 0x5bd6, 0x35, "LEVEL9-017-2" }, // Scapeghost, pt. 2 GD (Spectrum 128)
    { 0x97c0, 0x48, "LEVEL9-017-2" }, // Scapeghost, pt. 2 (Commodore 64 TO/MSX *converted*)
    { 0xbcb6, 0x7a, "LEVEL9-017-3" }, // Scapeghost, pt. 3 (Amiga/PC/ST)
    { 0x90ac, 0x68, "LEVEL9-017-3" }, // Scapeghost, pt. 3 (Commodore 64 Gfx)
    { 0x8a16, 0xcc, "LEVEL9-017-3" }, // Scapeghost, pt. 3 (Spectrum 48)
    { 0x51bc, 0xe3, "LEVEL9-017-3" }, // Scapeghost, pt. 3 GD (BBC)
    { 0x5860, 0x95, "LEVEL9-017-3" }, // Scapeghost, pt. 3 GD (Amstrad CPC/Spectrum +3)
    { 0x6fa8, 0xa4, "LEVEL9-017-3" }, // Scapeghost, pt. 3 GD (Spectrum 128)
    { 0xb60e, 0x6a, "LEVEL9-017-3" }, // Scapeghost, pt. 3 (Commodore 64 TO/MSX *converted*)
    { 0x5fab, 0x5c, "LEVEL9-018" }, // Snowball (Amstrad CPC)
    { 0x5fab, 0x2f, "LEVEL9-018" }, // Snowball (MSX)
    { 0x7b31, 0x6e, "LEVEL9-018" }, // Snowball /SD (Amiga/ST)
    { 0x67a3, 0x9d, "LEVEL9-018" }, // Snowball /SD (Apple ][)
    { 0x6bf8, 0x3f, "LEVEL9-018" }, // Snowball /SD (Atari)
    { 0x7363, 0x65, "LEVEL9-018" }, // Snowball /SD (Commodore 64/MSX)
    { 0x7b2f, 0x70, "LEVEL9-018" }, // Snowball /SD (Mac/PC/Spectrum 128/Amstrad CPC/Spectrum +3)
    { 0x6541, 0x02, "LEVEL9-018" }, // Snowball /SD (Spectrum 48)
    { 0x5834, 0x42, "LEVEL9-019-1" }, // The Archers, pt. 1 (BBC)
    { 0x765d, 0xcd, "LEVEL9-019-1" }, // The Archers, pt. 1 (Commodore 64/MSX)
    { 0x6ce5, 0x58, "LEVEL9-019-1" }, // The Archers, pt. 1 (Spectrum)
    { 0x56dd, 0x51, "LEVEL9-019-2" }, // The Archers, pt. 2 (BBC)
    { 0x6e58, 0x07, "LEVEL9-019-2" }, // The Archers, pt. 2 (Commodore 64)
    { 0x68da, 0xc1, "LEVEL9-019-2" }, // The Archers, pt. 2 (Spectrum)
    { 0x6e56, 0x17, "LEVEL9-019-2" }, // The Archers, pt. 2 (MSX)
    { 0x5801, 0x53, "LEVEL9-019-3" }, // The Archers, pt. 3 (BBC)
    { 0x7e98, 0x6a, "LEVEL9-019-3" }, // The Archers, pt. 3 (Commodore 64/MSX)
    { 0x6c67, 0x9a, "LEVEL9-019-3" }, // The Archers, pt. 3 (Spectrum)
    { 0x54a4, 0x01, "LEVEL9-019-4" }, // The Archers, pt. 4 (BBC)
    { 0x81e2, 0xd5, "LEVEL9-019-4" }, // The Archers, pt. 4 (Commodore 64/MSX)
    { 0x6d91, 0xb9, "LEVEL9-019-4" }, // The Archers, pt. 4 (Spectrum)
    { 0x5828, 0xbd, "LEVEL9-020" }, // Worm in Paradise (BBC)
    { 0x6d84, 0xf9, "LEVEL9-020" }, // Worm in Paradise (Commodore 64 *corrupt*)
    { 0x6d84, 0xc8, "LEVEL9-020" }, // Worm in Paradise (Commodore 64 *fixed*)
    { 0x6030, 0x47, "LEVEL9-020" }, // Worm in Paradise (Amstrad CPC)
    { 0x772b, 0xcd, "LEVEL9-020" }, // Worm in Paradise (Spectrum 128/MSX)
    { 0x546c, 0xb7, "LEVEL9-020" }, // Worm in Paradise (Spectrum 48)
    { 0x7cd9, 0x0c, "LEVEL9-020" }, // Worm in Paradise /SD (Amiga/ST)
    { 0x60dd, 0xf2, "LEVEL9-020" }, // Worm in Paradise /SD (Apple ][)
    { 0x6161, 0xf3, "LEVEL9-020" }, // Worm in Paradise /SD (Atari)
    { 0x788d, 0x72, "LEVEL9-020" }, // Worm in Paradise /SD (Commodore 64/MSX)
    { 0x7cd7, 0x0e, "LEVEL9-020" }, // Worm in Paradise /SD (Amstrad CPC/Mac/PC/Spectrum 128/Spectrum +3)
    { 0x5ebb, 0xf1, "LEVEL9-020" }, // Worm in Paradise /SD (Spectrum 48)
    { 0, 0, NULL }
};

/* Whole-file container entries: see struct l9filerec. */
static struct l9filerec l9_file_registry[] = {
    { 0xa96d, 0x5018, "LEVEL9-001-1" }, // Adrian Mole I, pt. 1 (Spectrum)
    { 0xa705, 0x3dda, "LEVEL9-001-2" }, // Adrian Mole I, pt. 2 (Spectrum)
    { 0xaec9, 0xcc6c, "LEVEL9-001-2" }, // Adrian Mole I, pt. 2 (Spectrum Z80, *corrupt* dump)
    { 0x2f900, 0xaec7, "LEVEL9-005" }, // Jewels of Darkness (Amstrad CPC/Spectrum +3 disk; 1st game Colossal Adventure)
    { 0x2f900, 0xbaa8, "LEVEL9-013" }, // Time and Magic GD (Amstrad CPC/Spectrum +3 disk; 1st game Lords of Time)
    { 0x2f900, 0xa151, "LEVEL9-013" }, // Time and Magic GD (Amstrad CPC/Spectrum +3 disk; side B)

    { 0x2f900, 0x924f, "LEVEL9-011-1" }, // Knight Orc (Amstrad CPC/Spectrum +3, game disk; 1st part)
    { 0x2f900, 0xe840, "LEVEL9-011-1" }, // Knight Orc (Amstrad CPC/Spectrum +3, graphics disk: only contains the file ALLPICS.PIC)
    { 0x17c38, 0xec57, "LEVEL9-014" }, // Price of Magik /T&M TZX (Spectrum 48)
    { 0x17ba0, 0xbb69, "LEVEL9-014" }, // Price of Magik /T&M TZX (Spectrum 128)
    // Whole-disk Extended CPC/Spectrum +3 disk images. The a-code is split
    // into separate GAMEDATn.DAT + ACODEn.ACD files and stored across
    // sector-interleaved tracks, so the scanner can't extract it; match the
    // whole image instead. IFID is that of the first game on the disk.
    { 0x2f900, 0x59bb, "LEVEL9-017-1" }, // Scapeghost (Amstrad CPC/Spectrum +3, game disk: GAMEDAT1-3/ACODE1-3)
    { 0x2f900, 0x37ba, "LEVEL9-017-1" }, // Scapeghost (Amstrad CPC/Spectrum +3, graphics disk: ALLPICS.PIC)
    { 0x2f900, 0xded3, "LEVEL9-009-1" }, // Gnome Rangeer (Amstrad CPC/Spectrum +3, graphics disk: ALLPICS.PIC)
    { 0x2f900, 0xbc42, "LEVEL9-018" }, // Silicon Dreams (Amstrad CPC/Spectrum +3 disk; 1st game Snowball)
    { 0xb433, 0xd55c, "LEVEL9-019-1" }, // The Archers, pt. 1 (Spectrum)
    { 0xb08e, 0x9a19, "LEVEL9-019-2" }, // The Archers, pt. 2 (Spectrum Z80, *corrupt* dump)
    { 0, 0, NULL }
};


static int32 read_l9_int(unsigned char *sf)
{
    return ((int32) sf[1]) << 8 | sf[0];
}

static int v2_recognition (unsigned char *sf, int32 extent, int32 *l, unsigned char *c)
{
    int32 i, j;
    for (i=0;i<extent-0x1c;i++)
        if ((read_l9_int(sf+i+4) == 0x0020) &&
            (read_l9_int(sf+i+0x0a) == 0x8000) &&
            (read_l9_int(sf+i+0x14) == read_l9_int(sf+i+0x16)))
        {
            *l=read_l9_int(sf+i+0x1c);
            if (*l && *l+i <=extent)
            {
                *c=0;
                for(j=0;j<=*l;j++)
                    *c+=sf[i+j];
                return 2;
            }
        }
    return 0;
}

static int v1_recognition(unsigned char *sf, int32 extent, char **ifid)
{
    int32 i;
    unsigned char a = 0xff, b = 0xff;
    
    for (i=0;i<extent-20;i++)
        if (memcmp(sf+i,"ATTAC",5)==0 && sf[i+5]==0xcb)
        {
            a = sf[i+6];
            break;
        }
    for (;i<(extent-20);i++)
        if (memcmp(sf+i,"BUNC",4)==0 && sf[i+4]==0xc8)
        {
            b = sf[i + 5];
            break;
        }
    if (a == 0xff && b == 0xff)
        return 0;
    if (a == 0x14 && b == 0xff) *ifid="LEVEL9-006";
    else if (a == 0x15 && b == 0x5d) *ifid="LEVEL9-013";
    else if (a == 0x15 && b == 0x6c) *ifid="LEVEL9-018";
    else if (a == 0x1a && b == 0x24) *ifid="LEVEL9-005";
    else if (a == 0x20 && b == 0x3b) *ifid="LEVEL9-003";
    else *ifid=NULL;
    return 1;
}

/* Locate embedded Level 9 V3/V4 a-code by scanning for its header signature.
 * Ported from the v3_recognition routine in Paul David Doherty's l9cut, which
 * runs three increasingly lenient passes:
 *   phase 1 - require two zero bytes bracketing the end of the data;
 *   phase 2 - as phase 1 but without the zero-padding requirement;
 *   phase 3 - "desperate mode": a looser structural test and no checksum.
 * On a match returns 3 (V3) or 4 (V4) depending on the a-code length, sets *l
 * to that length and *c to the a-code's stored 8-bit checksum byte. */
static int v3_recognition_phase (int phase,unsigned char *sf, int32 extent, int32 *l, unsigned char *c)
{
    int32 i, j, end;
    int found = 0;

    if (sf == NULL)
        return 0;

    for (i = 0; i < extent - 20 && !found; i++)
    {
        int match = 0;
        *l = read_l9_int(sf+i);
        end = *l + i;

        if (phase != 3)
        {
            /* Length in range, dictionary-length high byte (offset 0x0d) zero,
             * the data optionally ending in two zero bytes (phase 1 only), and
             * at least two of the first eight address-table entries forming a
             * consistent running sum. */
            int padded = (end >= 2 && end <= extent && sf[end-1] == 0 && sf[end-2] == 0) ||
                         (end <= extent-3 && sf[end+1] == 0 && sf[end+2] == 0);
            if (end <= extent && (phase == 2 || padded)
                && *l > 0x4000 && *l <= 0xdb00 && sf[i+0x0d] == 0)
            {
                int triples = 0;
                for (j = i; j < i + 16; j += 2)
                {
                    int32 sum = read_l9_int(sf+j) + read_l9_int(sf+j+2);
                    if (sum != 0 && sum == read_l9_int(sf+j+4))
                        triples++;
                }
                match = (triples > 1);
            }
        }
        else
        {
            /* Desperate mode: two consecutive consistent running sums in the
             * address table, then a 0x2a/0x2c opcode followed by three zeroes. */
            int32 w2 = read_l9_int(sf+i+2), w4 = read_l9_int(sf+i+4);
            int32 w6 = read_l9_int(sf+i+6);
            if (extent > 0x0fd0 && end <= extent
                && w2 != 0 && w4 != 0 && w2 + w4 == w6
                && w6 + read_l9_int(sf+i+8) == read_l9_int(sf+i+10)
                && (sf[i+18] == 0x2a || sf[i+18] == 0x2c)
                && sf[i+19] == 0 && sf[i+20] == 0 && sf[i+21] == 0)
                match = 1;
        }

        if (!match)
            continue;

        if (phase == 3)
        {
            *c = 0;
            found = 1;
        }
        else
        {
            /* Confirm the a-code's 8-bit checksum closes to zero. */
            char checksum = 0;
            *c = sf[end];
            for (j = i; j <= end; j++)
                checksum += sf[j];
            found = (checksum == 0);
        }
    }

    if (found) return *l < 0x8500 ? 3:4;
    return 0;
}

static char *get_l9_ifid(int32 length, unsigned char chk)
{
    int i;
    for(i=0;l9_registry[i].length;i++)
        if (length==l9_registry[i].length && chk==l9_registry[i].chk)
            return l9_registry[i].ifid;
    return NULL;
}

static uint16_t l9_file_sum16(unsigned char *sf, int32 extent)
{
    uint16_t c = 0;
    int32 i;
    for (i = 0; i < extent; i++) c += sf[i];
    return c;
}

/* Match the l9_file_registry entries against the raw input buffer. The a-code
 * recognisers can't see through tape/snapshot/disk wrappers, so for those we
 * identify the container by (length, 16-bit sum) — the same approach the scott
 * and taylor babel modules use. */
static const struct l9filerec *get_l9_file_rec(unsigned char *sf, int32 extent)
{
    int i;
    int computed = 0;
    uint16_t chk = 0;
    for (i = 0; l9_file_registry[i].length; i++) {
        if (extent != l9_file_registry[i].length) continue;
        if (!computed) {
            chk = l9_file_sum16(sf, extent);
            fprintf(stderr, "Checksum is 0x%x\n", chk);
            computed = 1;
        }
        if (chk == l9_file_registry[i].file_chk) return &l9_file_registry[i];
    }
    return NULL;
}

/* Run the a-code recognisers over a flat memory image, in the same order the
 * module has always used. Returns the Level 9 version (0 if nothing found) and
 * sets *ifid to the matched IFID, or NULL when only the version could be
 * established (desperate mode / unknown checksum). */
static int scan_l9_acode(unsigned char *sf, int32 extent, char **ifid)
{
    int i;
    int32 l = 0;
    unsigned char c = 0;
    if (v2_recognition(sf,extent, &l, &c)) { *ifid=get_l9_ifid(l,c); return 2; }
    l=0; c=0;
    i=v3_recognition_phase(1,sf,extent, &l, &c);
    if (i) { *ifid=get_l9_ifid(l,c); return i; }
    if (v1_recognition(sf,extent, ifid)) return 1;
    l=0; c=0;
    i=v3_recognition_phase(2,sf,extent, &l, &c);
    if (i) { *ifid=get_l9_ifid(l,c); return i; }
    i=v3_recognition_phase(3,sf,extent, &l, &c);
    *ifid=NULL;
    return i;
}

/* Decode one Z80-snapshot memory block (ED ED run-length encoding) from
 * in[0..inlen) into out[0..outsize), zero-padding any shortfall. Faithful to
 * the decoder in Paul David Doherty's l9cut (z80_block), with added output
 * bounds checks so a malformed snapshot can't overrun the buffer. */
static void z80_decode_block(const unsigned char *in, int32 inlen,
                             unsigned char *out, int32 outsize)
{
    int32 x = 0, y = 0;
    while (x < inlen && y < outsize) {
        if (in[x] != 237) { out[y++] = in[x]; x++; }
        else if (x+1 >= inlen) { out[y++] = in[x]; x++; }
        else if (in[x+1] != 237) {
            out[y++] = 237;
            if (y < outsize) out[y++] = in[x+1];
            x += 2;
        } else {
            if (x+3 >= inlen) break;
            int count = in[x+2];
            unsigned char val = in[x+3];
            while (count-- > 0 && y < outsize) out[y++] = val;
            x += 4;
        }
    }
    while (y < outsize) out[y++] = 0;
}

/* For a v2/v3 Z80 snapshot, walk the page list starting at `pos` and decode the
 * 16K page whose page-type byte equals `wanted` into out[0..16384). */
static int z80_find_page(const unsigned char *sf, int32 extent, int32 pos,
                         int wanted, unsigned char *out)
{
    while (pos + 3 <= extent) {
        int32 len = sf[pos] | (sf[pos+1] << 8);
        int type = sf[pos+2];
        int32 data = pos + 3;
        if (type == wanted) {
            if (len == 0xffff) {
                int32 n = extent - data; if (n > 16384) n = 16384;
                if (n > 0) memcpy(out, sf+data, n);
            } else {
                int32 n = len; if (n > extent - data) n = extent - data;
                z80_decode_block(sf+data, n, out, 16384);
            }
            return 1;
        }
        if (len == 0xffff) len = 16384;
        pos = data + len;
    }
    return 0;
}

/* If 'sf' is a Z80 Spectrum snapshot, rebuild the visible 48K memory image
 * (0x4000-0xffff) into a freshly malloc'd 49152-byte buffer so the a-code
 * recognisers can see through Z80 compression. Returns NULL when the buffer is
 * not a recognisable Z80 snapshot. Mirrors l9cut's dec_open_z80. */
static unsigned char *z80_to_image(const unsigned char *sf, int32 extent, int32 *out_len)
{
    unsigned char *img;
    if (extent < 34) return NULL;

    if ((sf[6] | (sf[7] << 8)) != 0) {
        /* v1.45: a single 48K block follows the 30-byte header */
        int flags = sf[12];
        if (flags == 255) flags = 1;
        if ((img = malloc(49152)) == NULL) return NULL;
        if (flags & 0x20) {
            /* compressed up to the trailing 00 ED ED 00 end marker */
            int32 inlen = extent - 34;
            if (inlen < 0) { free(img); return NULL; }
            z80_decode_block(sf+30, inlen, img, 49152);
        } else {
            int32 inlen = extent - 30;
            if (inlen > 49152) inlen = 49152;
            memcpy(img, sf+30, inlen);
            if (inlen < 49152) memset(img+inlen, 0, 49152-inlen);
        }
        *out_len = 49152;
        return img;
    }

    /* v2/v3: extra header length distinguishes the sub-version */
    {
        int xhlen = sf[30] | (sf[31] << 8);
        int32 seekseed;
        int firsttype, pages[3], k;
        if (xhlen == 23) seekseed = 55;                 /* v2.01 */
        else if (xhlen == 54 || xhlen == 55) seekseed = 86; /* v3.0x */
        else return NULL;
        if (seekseed + 3 > extent) return NULL;
        firsttype = sf[seekseed+2];
        if (firsttype == 4) {       /* 48K snapshot: pages 5-1-2 */
            pages[0]=8; pages[1]=4; pages[2]=5;
        } else if (firsttype == 3) {/* 128K snapshot: banks 5-2-0 */
            pages[0]=8; pages[1]=5; pages[2]=3;
        } else return NULL;
        if ((img = malloc(49152)) == NULL) return NULL;
        memset(img, 0, 49152);
        for (k = 0; k < 3; k++)
            z80_find_page(sf, extent, seekseed, pages[k], img + k*16384);
        *out_len = 49152;
        return img;
    }
}

static int get_l9_version(unsigned char *sf, int32 extent, char **ifid)
{
    int v, rawver;
    char *rawifid;
    int32 dlen = 0;
    unsigned char *img;
    const struct l9filerec *rec = get_l9_file_rec(sf, extent);
    /* A whole-file match always carries an IFID; the return value only needs
     * to be nonzero (it is never surfaced as a version number). */
    if (rec) { *ifid = rec->ifid; return 1; }

    /* First scan the raw buffer (catches bare a-code and uncompressed
     * tape/snapshot dumps). */
    v = scan_l9_acode(sf, extent, ifid);
    if (v && *ifid) return v;
    rawver = v; rawifid = *ifid;

    /* Fall back to decoding a Z80 snapshot and scanning the decompressed
     * image. Only accept a positively identified game so a stray buffer that
     * merely looks like a Z80 header can't produce a false claim. */
    img = z80_to_image(sf, extent, &dlen);
    if (img) {
        char *zifid = NULL;
        int zv = scan_l9_acode(img, dlen, &zifid);
        free(img);
        if (zv && zifid) { *ifid = zifid; return zv; }
    }

    *ifid = rawifid;
    return rawver;
}

static int32 claim_story_file(void *story, int32 extent)
{
    char *ifid=NULL;
    if (get_l9_version((unsigned char *) story,extent, &ifid)) {
        if (ifid) return VALID_STORY_FILE_RV;
        else return NO_REPLY_RV;
    }
    return INVALID_STORY_FILE_RV;
}


static int32 get_story_file_IFID(void *story_file, int32 extent, char *output, int32 output_extent)
{
    char *ifid=NULL;
    int i=get_l9_version((unsigned char *)story_file, extent, &ifid);
    if (!i) return INVALID_STORY_FILE_RV;
    if (ifid)
    {
        ASSERT_OUTPUT_SIZE((signed) strlen(ifid)+1);
        memcpy(output, ifid, strlen(ifid) + 1);
        return 1;
    }
    ASSERT_OUTPUT_SIZE(10);
    sprintf(output,"LEVEL9-%d-",i);
    return INCOMPLETE_REPLY_RV;
}

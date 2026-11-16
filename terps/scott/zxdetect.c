/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  zxdetect.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Detection of ZX Spectrum game files: .z80 and .sna snapshots, .tap and
 *  .tzx tapes, Alkatraz-protected tapes and Parsec-compressed snapshots.
 */

#include <stdlib.h>
#include <string.h>

#include "scott.h"
#include "scott_meta.h"

#include "decompressz80.h"
#include "scott_game_info.h"
#include "uk_loader.h"
#include "zxdetect.h"

/* ZX_RAM_BASE (0x4000, from decompressz80.h) is where the RAM of the ZX
   Spectrum begins and where loaded programs live. Used to translate Z80
   addresses stored in saved .z80 snapshots to offsets into our in-memory
   file buffer. */

/* Decompress a Parsec-packed ZX Spectrum game file. Four "scene releases"
   by Parsec (Denmark) use this RLE scheme: Pirate Adventure, Voodoo Castle,
   Strange Odyssey, and Buckaroo Banzai. Operates backwards through memory,
   expanding either literal runs or repeated byte-pairs. */
static uint8_t *DecompressParsec(uint8_t *start, uint8_t *end, uint8_t *dataptr)
{
    if (dataptr - 3 < entire_file) {
        return NULL;
    }
    uint8_t *pos = start;

    while (pos != end && dataptr - 3 > entire_file) {
        if (pos <= entire_file) {
            return NULL;
        }
        uint8_t *nextptr = dataptr - 2;
        if ((*dataptr & 0x80) == 0) {
            int repeats = READ_LE_UINT16(dataptr - 1);
            for (int i = 0; i < repeats && nextptr > entire_file && pos > entire_file; i++) {
                *pos = *nextptr;
                pos--;
                nextptr--;
            }
        } else {
            uint8_t val1 = *(dataptr - 1);
            uint8_t val2 = *(dataptr - 2);
            nextptr = dataptr - 3;
            int repeats = *dataptr & 0x7f;
            for (int i = 0; i < repeats && pos > entire_file; i++) {
                *pos = val1;
                pos--;
                *pos = val2;
                pos--;
            }
        }
        dataptr = nextptr;
    }
    return dataptr;
}

/* MARK: Alkatraz tape decryption */

/* Some commercial Scott Adams tapes (notably the U.S. Gold "Scott Adams
   Scoops" compilation) are protected with the Appleby Associates "Alkatraz"
   loader, which decrypts the game with a rolling-key XOR as it streams from
   tape, so the raw .tzx contains no recognisable dictionary. We reverse the
   final decryption pass here (the same scheme TaylorMade's extracttape.c
   uses) to reconstruct the loaded Spectrum memory image, which the normal ZX
   dictionary scan below can then identify.

   The per-release parameters are recovered by tracing the loader in an
   emulator: break on the store that writes a known decrypted byte, then read
   the key seed (loacon), the destination/count and the source pointer.

   The DeAlkatraz decryption primitive itself lives in the shared decompressz80
   library (and is also used by the TaylorMade interpreter). */

/* One game on an Alkatraz compilation tape: its title (shown in the selection
   menu) and the per-game key parameters. source_offset/target/size are shared
   by every game on a tape, so they live in AlkatrazTape. */
typedef struct {
    const char *title;
    int tzx_block;     /* number of the data block the game is encrypted in */
    uint8_t loacon;    /* initial rolling key */
    uint8_t add1;      /* key-evolution constants */
    uint8_t add2;
} AlkatrazGame;

/* One Alkatraz-protected release, identified by exact file length +
   checksum. */
typedef struct {
    int32_t file_length;   /* exact .tzx length identifying the release */
    uint16_t file_chk;     /* sum-of-bytes checksum of the whole file */
    size_t source_offset;  /* offset of the encrypted data within each block */
    uint16_t target;       /* Spectrum address the decrypt writes to */
    uint16_t size;         /* number of bytes to decrypt */
    int num_games;         /* >1 ⇒ prompt the player to choose */
    const AlkatrazGame *games;
} AlkatrazTape;

/* Scott Adams Scoops (1987)(U.S. Gold).tzx — four games behind one Alkatraz
   loader (shared source offset 0x11, target 0x5B00, size 0x4500). Each game's
   seed + key-evolution constants were recovered by tracing the loader and
   verified against the reference snapshots: the decrypted dictionary lands at
   the address each game's GameInfo expects (Pirate 0x869A, Voodoo 0x8752,
   Strange Odyssey 0x899A, Buckaroo 0x8C54). */
static const AlkatrazGame scoops_games[] = {
    { "Pirate Adventure",  3, 0xad, 0xfd, 0xff },
    { "Voodoo Castle",     7, 0x1e, 0xfe, 0xbe },
    { "Strange Odyssey",  11, 0x5a, 0x9c, 0xdc },
    { "Buckaroo Banzai",  15, 0xdb, 0xb3, 0x21 },
};

static const AlkatrazTape alkatraz_tapes[] = {
    { 0x16644, 0x3982, 0x11, 0x5b00, 0x4500, 4, scoops_games },
    { 0, 0, 0, 0, 0, 0, NULL }
};

/* If entire_file is a known Alkatraz-protected tape, let the player pick a
   game (compilations carry several), decrypt it into a fresh Spectrum memory
   image (offset 0 == address 0x4000, as DecompressZ80 produces) and adopt it
   as entire_file. Returns 1 on success, leaving the ZX dictionary scan to
   identify the chosen game. */
static int DeAlkatrazTape(void)
{
    if (file_length < 16 || memcmp(entire_file, "ZXTape!\x1a", 8) != 0)
        return 0;

    uint16_t sum = 0;
    for (size_t i = 0; i < file_length; i++)
        sum += entire_file[i];

    const AlkatrazTape *tape = NULL;
    for (int i = 0; alkatraz_tapes[i].file_length != 0; i++) {
        if ((int32_t)file_length == alkatraz_tapes[i].file_length
            && sum == alkatraz_tapes[i].file_chk) {
            tape = &alkatraz_tapes[i];
            break;
        }
    }
    if (tape == NULL)
        return 0;

    int choice = 0;
    if (tape->num_games > 1) {
        const char *titles[16];
        int n = tape->num_games < 16 ? tape->num_games : 16;
        for (int i = 0; i < n; i++)
            titles[i] = tape->games[i].title;
        choice = SelectGameFromMenu(
            "This tape contains several games. Which would you like to play?",
            titles, n);
    }
    const AlkatrazGame *game = &tape->games[choice];

    size_t blocklen = 0;
    uint8_t *block = GetTZXBlock(game->tzx_block, entire_file, file_length, &blocklen);
    if (block == NULL)
        return 0;

    uint8_t loacon = game->loacon;
    const AlkatrazKey schedule = { game->add1, game->add2, 0, 0 };
    uint8_t *mem = ZXNewMemory();
    int ok = mem != NULL && DeAlkatraz(block, blocklen, tape->source_offset,
        mem, tape->target, tape->size, &loacon, &schedule);
    free(block);
    if (!ok) {
        free(mem);
        return 0;
    }

    uint8_t *image = MemAlloc(ZX_MEMORY_SIZE - ZX_RAM_BASE);
    memcpy(image, mem + ZX_RAM_BASE, ZX_MEMORY_SIZE - ZX_RAM_BASE);
    free(mem);

    free(entire_file);
    entire_file = image;
    file_length = 0xc000;
    return 1;
}

/* Try loading entire_file as each game that uses dictionary type dict_type.
   On success, copies the matching GameInfo into *Game and returns the result
   of TryLoading(). */
static GameIDType TryGamesWithDictionary(DictionaryType dict_type, size_t dict_offset)
{
    for (int i = 0; games[i].Title != NULL; i++) {
        if (games[i].dictionary == dict_type) {
            GameIDType result = TryLoading(entire_file, file_length, &games[i], dict_offset);
            if (result != UNKNOWN_GAME) {
                *Game = games[i];
                return result;
            }
        }
    }
    return UNKNOWN_GAME;
}

/* Detect and load a ZX Spectrum game file. Handles .z80 snapshot
   decompression, dictionary signature scanning, and Parsec RLE
   decompression (a two-pass scheme using Z80 register values
   stored at fixed offsets in the snapshot). */
GameIDType DetectZXSpectrum(void)
{
    GameIDType detectedGame = UNKNOWN_GAME;
    int was_z80_snapshot = 0;

    /* Capture a loading screen to show as a title image. For .tap/.tzx
       tapes the SCREEN$ is a data block we extract from the raw image,
       so grab it before DecompressZ80() (a no-op on tapes) runs. */
    int is_tzx = (file_length > 8 && memcmp(entire_file, "ZXTape!\x1a", 8) == 0);
    int looks_like_tap = (!is_tzx && file_length > 21 &&
        entire_file[0] == 0x13 && entire_file[1] == 0x00 && entire_file[2] == 0x00);
    if (is_tzx || looks_like_tap)
        ZXLoadingScreen = FindTapeLoadingScreen(entire_file, file_length, is_tzx);

    /* Alkatraz-protected tapes carry no plaintext dictionary. If this is a
       known one, decrypt the embedded game image now (replacing entire_file
       with a 0x4000-based Spectrum image) and skip the .z80 path below. */
    int decrypted = (is_tzx && DeAlkatrazTape());

    /* Tapes and .sna snapshots are not .z80 snapshots: read as one they are
       only rejected, with a libspectrum error on stderr. A .sna has no
       signature, but it does have one of two sizes. */
    int is_sna = (file_length == 49179 || file_length == 131103 || file_length == 147487);
    int not_z80 = (decrypted || is_tzx || looks_like_tap || is_sna);

    uint8_t *uncompressed = not_z80 ? NULL : DecompressZ80(entire_file, &file_length);
    if (uncompressed != NULL) {
        was_z80_snapshot = 1;
        free(entire_file);
        entire_file = uncompressed;
        /* In a decompressed 48K snapshot, offset 0 maps to Spectrum
           address 0x4000 (the display file), so the first 6912 bytes are
           whatever screen was visible when the snapshot was saved. */
        if (ZXLoadingScreen == NULL && file_length >= ZX_SCREEN_SIZE) {
            ZXLoadingScreen = MemAlloc(ZX_SCREEN_SIZE);
            memcpy(ZXLoadingScreen, entire_file, ZX_SCREEN_SIZE);
        }
    }

    /* Snapshots saved at a text prompt (e.g. "Resume a saved game?") have
       no picture, just black text on white. Don't show those as a title. */
    if (ZXLoadingScreen != NULL && ZXScreenIsBlackOnWhite(ZXLoadingScreen)) {
        free(ZXLoadingScreen);
        ZXLoadingScreen = NULL;
    }

    size_t dict_offset;

    DictionaryType dict_type = GetId(entire_file, file_length, &dict_offset);

    if (dict_type == NOT_A_GAME)
        return UNKNOWN_GAME;

    detectedGame = TryGamesWithDictionary(dict_type, dict_offset);

    /* If no dictionary was found and this was a .z80 snapshot, try
       Parsec RLE decompression. The decompression parameters (start/end
       addresses) are stored in saved Z80 registers at fixed offsets.
       Addresses are adjusted by -ZX_RAM_BASE (0x4000) to convert from
       the Spectrum's memory map to the file buffer. */
    if (!detectedGame && was_z80_snapshot) {
        uint8_t *file_data = entire_file;
        /* HL: source pointer for the first Parsec pass (the compressed
           data inside the snapshot). */
        uint16_t HL = READ_LE_UINT16(file_data + 0x1b42) - ZX_RAM_BASE;
        uint8_t *first_pass_result = 0;
        if (HL < file_length)
            first_pass_result = DecompressParsec(&file_data[0x0fff], &file_data[0x07ff], &file_data[HL]);
        if (first_pass_result) {
            /* BC/DE: output start/end for the second Parsec pass, which
               unpacks the first-pass result into its final position. */
            uint16_t BC = READ_LE_UINT16(file_data + 0x1b48) - ZX_RAM_BASE;
            uint16_t DE = READ_LE_UINT16(file_data + 0x1b4b) - ZX_RAM_BASE;
            DecompressParsec(&file_data[BC], &file_data[DE], first_pass_result);
            dict_type = GetId(entire_file, file_length, &dict_offset);
            if (dict_type == NOT_A_GAME)
                Fatal("Unsupported game!");
            if (TryGamesWithDictionary(dict_type, dict_offset) != UNKNOWN_GAME)
                detectedGame = Game->gameID;
        }
    }

    if (!detectedGame)
        Fatal("Unsupported game!");

    return detectedGame;
}


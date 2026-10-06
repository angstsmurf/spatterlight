// C-renderer side of the C64 "tiny" (mini-C64) SAGA bitmap test. Feeds raw image
// blobs (exactly the bytes the interpreter's C64 decrunch pipeline hands to
// DrawMiniC64 — see extract_images_c64 / groundtruth_c64/README.md) to
// saga/c64_small.c and captures every plotted pixel's *displayed colour* into a
// 320x200 RGB grid. That grid is byte-compared against a golden decoded from a
// real VICE screenshot of the same room (see c64_decode_png.py).
//
// We capture displayed RGB (slot_rgb[slot] at plot time), not the raw 2-bit
// slot, because a room is composited from several images — a background bitmap
// plus object overlays (which re-issue SetColor with their own palette) plus
// monochrome white sprites — so the same slot index means different colours in
// different regions. Comparing displayed colour is palette-change-safe and is
// what the player actually sees. The golden stores those colours in the C
// renderer's own colour space (c64_decode_png.py classifies each VICE pixel to
// the nearest renderer colour), so cmp is a pure RGB-equality check and stays
// self-contained / reproducible.
//
// c64_small.c is unity-included to reach the file-static DrawMiniC64ImageFromData
// / DrawSprite and the sprites[] table; the renderer's own DrawMiniC64 dispatch
// (sprite vs bitmap overlay, per object index) is mirrored in render_spec().
//
// A render spec is a small text file (paths relative to the spec's dir):
//     <background.dat>
//     <objindex> <object.dat>      ... zero or more overlay/sprite objects
//
//   usage:
//     c64test dump <spec>                 ASCII-art the composited render
//     c64test pal  <spec>                 print the distinct displayed colours
//     c64test grid <spec> <out.grid>      write the raw RGB grid
//     c64test cmp  <spec> <golden.c64>    render + byte-compare to golden

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

// ---- displayed-colour capture (rgbgrid.h) ------------------------------------
#define C64_W 320
#define C64_H 200

static uint32_t slot_rgb[8];             // SetColor sinks: 0..3 bitmap, 5 sprite

#define RG_W C64_W
#define RG_H C64_H
#define RG_LABEL "c64"
#define RG_SLOT_RGB(slot) slot_rgb[(slot) & 7]
#define RG_DUMP_XSTEP 4                  // double-width pixels
#define RG_C64_STYLE 1
#include "rgbgrid.h"

void PutPixel(glsi32 x, glsi32 y, int32_t color) { plot((int)x, (int)y, (int)color); }
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color) {
    plot((int)x, (int)y, (int)color);
    plot((int)x + 1, (int)y, (int)color);
}
void SetColor(int32_t index, glui32 color) {
    if (index >= 0 && index < 8) slot_rgb[index] = color & 0xffffff;
}

// c64_small.c calls this (the original lives in c64a8draw.c); copied verbatim so
// we don't drag in the whole c64a8draw translation unit.
int DrawPatternAndAdvancePos(int x, int *y, uint8_t pattern) {
    uint8_t mask = 0xc0;
    for (int i = 6; i >= 0; i -= 2) {
        int color = (pattern & mask) >> i;
        PutDoublePixel(x, *y, color);
        mask >>= 2;
        x += 2;
    }
    (*y)++;
    return x - 8;
}

// Unity-include the renderer to reach the file-static DrawMiniC64ImageFromData,
// DrawSprite and sprites[].
#include "c64_small.c"

// ---- loader stubs the unity-included c64_small.c references ------------------
// Only DrawMiniC64ImageFromData / DrawSprite are called; the file's loader path
// (handle_all_in_one) and higher-level DrawMiniC64 are compiled but never run.
GameInfo *Game = NULL;
Header GameHeader;
MachineType CurrentSys = SYS_UNKNOWN;
USImage *USImages = NULL;
int ImageWidth = 0, ImageHeight = 0;
void *MemAlloc(size_t s) { return malloc(s); }
void *MemCalloc(size_t s) { return calloc(1, s); }
void *MemRealloc(void *p, size_t n) { return realloc(p, n); }
USImage *NewImage(void) { return NULL; }
GameIDType LoadBinaryDatabase(uint8_t *d, size_t l, GameInfo info, int ds) {
    (void)d; (void)l; (void)info; (void)ds; return UNKNOWN_GAME;
}

// ---- helpers -----------------------------------------------------------------
// Render one object: a hardware sprite (if its index is in sprites[]) drawn at
// its fixed screen position, else a self-positioning bitmap overlay — exactly
// the dispatch DrawMiniC64 does for Pirate Adventure room objects.
static void render_object(int objindex, uint8_t *blob, size_t sz) {
    for (int i = 0; sprites[i].index != 0; i++) {
        if (sprites[i].index == objindex) {
            USImage img = (USImage){0};
            img.imagedata = blob;
            img.datasize = sz;
            DrawSprite(&img, i);
            return;
        }
    }
    DrawMiniC64ImageFromData(blob, sz);
}

static void draw_spec_image(int isobj, int objindex, uint8_t *blob, size_t sz) {
    if (isobj) render_object(objindex, blob, sz);
    else       DrawMiniC64ImageFromData(blob, sz);
}

// Parse a render spec and composite it into `grid`. Returns 0 on success.
static int render_spec(const char *specpath) {
    rg_clear();
    memset(slot_rgb, 0, sizeof slot_rgb);
    return rg_parse_spec(specpath, NULL, draw_spec_image) < 0;
}

int main(int argc, char **argv) {
    return rg_main(argc, argv, render_spec, compare_golden);
}

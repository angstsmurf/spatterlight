// C-renderer side of the C64 / Atari 8-bit *full* bitmap test. Feeds raw image
// blobs (exactly the bytes the interpreter's loader hands to DrawC64A8Image —
// see c64extract / groundtruth_c64a8/README.md) to common_sagadraw/c64a8draw.c
// and captures every plotted pixel's *displayed colour* into a 320x200 RGB grid.
// That grid is byte-compared against a golden decoded from a real VICE
// screenshot of the same room (reuses c64_decode_png.py — same C64R1/C64C2
// formats and offset-search alignment as the mini-C64 test).
//
// Unlike the mini-C64 games (saga/c64_small.c, one packed file), the SagaPlus
// C64/Atari titles (Spider-Man, Buckaroo Banzai, …) ship the database and the
// images separately and decode their rooms with the RLE multicolor renderer in
// c64a8draw.c:DrawC64A8ImageFromData. That file is unity-included here to reach
// it; we supply the plotting sinks (PutDoublePixel / SetColor) so each plot is
// captured as displayed RGB, exactly like c64test.
//
// A render spec is a small text file (paths relative to the spec's dir):
//     [!atari8]                     optional: render with is_c64=0 (Atari 8-bit)
//     [!voodoo]                     optional: voodoo_or_count loader variant
//     <background.dat>              room background (IMG_ROOM adjustment)
//     <obj> <object.dat>           ... zero or more object overlays
//
//   usage:
//     c64a8test dump <spec>                 ASCII-art the composited render
//     c64a8test pal  <spec>                 print the distinct displayed colours
//     c64a8test grid <spec> <out.grid>      write the raw RGB grid
//     c64a8test cmp  <spec> <golden.c64>    render + byte-compare to golden

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

// ---- displayed-colour capture ------------------------------------------------
#define C64_W 320
#define C64_H 200

static uint32_t slot_rgb[16];            // SetColor sinks (palette slots 0..4)

// The original C64 display hides the top/bottom picture rows that fall in the
// PAL overscan border (Spider-Man's room images are drawn 2px into the top
// border, so their top two rows show as black, not as picture). Those rows are
// not part of the visible image, so a spec can mark them clipped (!cliptop N /
// !clipbottom N); we then never plot them and they drop out of the comparison.
static int g_cliptop = 0, g_clipbottom = 0;

#define RG_W C64_W
#define RG_H C64_H
#define RG_LABEL "c64a8"
#define RG_SLOT_RGB(slot) slot_rgb[(slot) & 15]
#define RG_CLIP(x, y) ((y) < g_cliptop || (y) >= C64_H - g_clipbottom)
#define RG_DUMP_XSTEP 4                  // double-width pixels
#define RG_C64_STYLE 1
#include "rgbgrid.h"

void PutPixel(glsi32 x, glsi32 y, int32_t color) { plot((int)x, (int)y, (int)color); }
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color) {
    plot((int)x, (int)y, (int)color);
    plot((int)x + 1, (int)y, (int)color);
}
void SetColor(int32_t index, glui32 color) {
    if (index >= 0 && index < 16) slot_rgb[index] = color & 0xffffff;
}

// c64a8draw.c declares this extern but never reads it in the bitmap path.
glui32 pal[16];

// Unity-include the renderer to reach DrawC64A8ImageFromData (+ its file-local
// DrawPatternAndAdvancePos / TranslateColorC64 / TranslateColorAtari8).
#include "c64a8draw.c"

// ---- adjustments mirror ------------------------------------------------------
// C64A8AdjustScott (saga/c64a8scott.c) clips the right edge per image usage. For
// a C64 room (cropright == 0) it returns width-1; an object overlay is left
// unchanged. We replicate just that geometry (no glk window / pixel_size
// scaling, which only affects on-screen blit, not the plotted grid).
static int g_is_room = 1;
static int adjust_room(int width, int height, int *x_origin) {
    (void)height; (void)x_origin;
    return g_is_room ? width - 1 : width;
}

// ---- helpers -----------------------------------------------------------------
static int g_is_c64 = 1;
static int g_voodoo = 0;

static int spec_directive(const char *p, int lineno) {
    if (strcmp(p, "!atari8") == 0) g_is_c64 = 0;
    else if (strcmp(p, "!voodoo") == 0) g_voodoo = 1;
    else if (strncmp(p, "!cliptop ", 9) == 0) g_cliptop = atoi(p + 9);
    else if (strncmp(p, "!clipbottom ", 12) == 0) g_clipbottom = atoi(p + 12);
    else { fprintf(stderr, "spec line %d: unknown directive %s\n", lineno + 1, p); return 1; }
    return 0;
}

static void draw_spec_image(int isobj, int objindex, uint8_t *blob, size_t sz) {
    (void)objindex;
    g_is_room = !isobj;
    DrawC64A8ImageFromData(blob, sz, g_voodoo, adjust_room, g_is_c64);
}

// Parse a render spec (directives: !atari8, !voodoo, !cliptop N, !clipbottom N)
// and composite it into `grid`. Returns 0 on success.
static int render_spec(const char *specpath) {
    rg_clear();
    memset(slot_rgb, 0, sizeof slot_rgb);
    g_is_c64 = 1; g_voodoo = 0; g_cliptop = 0; g_clipbottom = 0;

    int drew = rg_parse_spec(specpath, spec_directive, draw_spec_image);
    if (drew < 0) return 1;
    if (!drew) { fprintf(stderr, "spec %s drew nothing\n", specpath); return 1; }
    return 0;
}

int main(int argc, char **argv) {
    return rg_main(argc, argv, render_spec, compare_golden);
}

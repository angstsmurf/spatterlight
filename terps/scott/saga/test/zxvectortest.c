// C-renderer side of the ZX Spectrum Howarth/vector byte-exact test (the
// "Mysterious Adventures" line-drawing format, picture_format_version 99 —
// Golden Baton, Arrow of Death, …). Feeds one picture's vector opcode stream
// (dumped by build/zxextract from the real DetectGame -> LoadVectorData) to
// ai_uk/line_drawing.c:DrawHowarthVectorPicture and captures every plotted
// pixel's displayed colour into a 256x96 RGB grid, byte-compared to a golden
// decoded from a real ZX screenshot (same C64R1/C64C2 pipeline as the others).
//
// Like the Irmak test, the vector graphics never reach USImages, so the data is
// dumped by zxextract (the LineImages[] globals) rather than extract_images.
// line_drawing.c is unity-included to reach DrawHowarthVectorPicture + its
// file-statics; we supply the PutPixel/RectFill sinks + the loader globals it
// touches (entire_file/file_length/Game/VectorState/gli_slowdraw) and link
// palette.c + ringbuffer.c.
//
// A golden dir holds: pic<NNN>.dat (vector stream) and render.txt
// ("palette N", "picfile ...", "bg N").
//
//   usage: zxvectortest dump|grid|cmp <dir> [out|golden]

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "scott.h"
#include "palette.h"
#include "line_drawing.h"
#include "vector_common.h"

#define ZX_W 256
#define ZX_H 96

#define RG_W ZX_W
#define RG_H ZX_H
#define RG_LABEL "zxvec"
#define RG_SLOT_RGB(color) pal[(color) & 15]
#define RG_READ_ALLOW_EMPTY 1
#include "rgbgrid.h"

void PutPixel(glsi32 x, glsi32 y, int32_t color) { plot((int)x, (int)y, (int)color); }
void PutPixelWithWidth(glsi32 x, glsi32 y, int32_t color, int w) {
    for (int i = 0; i < w; i++) plot((int)x + i, (int)y, (int)color);
}
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color) { PutPixelWithWidth(x, y, color, 2); }
void RectFill(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color) {
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            plot((int)x + xx, (int)y + yy, (int)color);
}
void SetColor(int32_t index, glui32 color) { if (index >= 0 && index < 16) pal[index] = color & 0xffffff; }

// Loader globals line_drawing.c reaches into.
uint8_t *entire_file = NULL;
size_t file_length = 0;
GameInfo *Game = NULL;
Header GameHeader;

// VectorState normally lives in saga/vector_common.c, which drags in the whole
// Apple/Atari vector subsystem; we only need this one global.
VectorStateType VectorState = NO_VECTOR_IMAGE;

void Fatal(const char *msg) { fprintf(stderr, "Fatal: %s\n", msg); exit(1); }

// Unity-include the renderer (provides LineImages, VectorState lives in
// vector_common.c which we link).
#include "line_drawing.c"

static int render_dir(const char *dir) {
    rg_clear();

    char path[1200], picfile[256] = "";
    int palette = 2, bg = 0;
    const rg_kv keys[] = { { "palette", &palette }, { "bg", &bg }, { NULL, NULL } };
    if (rg_read_render_txt(dir, keys, picfile, sizeof picfile)) return 1;

    size_t psz = 0;
    snprintf(path, sizeof path, "%s/%s", dir, picfile);
    uint8_t *picbuf = read_file_ex(path, &psz, 1);
    if (!picbuf) { fprintf(stderr, "cannot read pic %s\n", path); return 1; }

    // line_drawing.c reads entire_file/file_length for its end-of-data bounds
    // check; point them at the picture buffer itself.
    entire_file = picbuf;
    file_length = psz;

    palchosen = (palette_type)palette;
    DefinePalette();

    static line_image one;
    one.data = picbuf;
    one.size = psz;
    one.bgcolour = bg;
    LineImages = &one;

    VectorState = NO_VECTOR_IMAGE;
    vector_image_shown = -1;
    DrawHowarthVectorPicture(0);   // builds + (gli_slowdraw==0) flushes all pixels
    return 0;
}


int main(int argc, char **argv) {
    return rg_main(argc, argv, render_dir, compare_golden);
}

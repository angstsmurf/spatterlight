// C-renderer side of the ZX Spectrum Irmak/tile byte-exact test. Feeds the
// exact loader state the interpreter hands aiukgraphics/irmak.c — the 256-tile
// font, one picture's tile/attribute data stream, and the picture geometry
// (all dumped by build/zxextract, which runs the real DetectGame ->
// SagaGraphicsSetup) — to DrawPictureNumber() and captures every plotted
// pixel's displayed colour into a 256x96 RGB grid (the ZX picture area is
// 32x12 tiles). That grid is byte-compared against a golden decoded from a real
// ZX Spectrum screenshot, reusing the same C64R1 grid / C64C2 golden format and
// offset-search decoder as the C64/Atari tests (c64a8_decode_png.py) — or
// against a raw SCREEN$ dump (.scr, 6912 bytes, as zx_capture.lua writes them),
// whose colours are looked up in the renderer's own pal[]. Only the pixels the
// renderer plots are compared, so the dump may show the picture as an overlay
// on another one.
//
// The ZX graphics never reach USImages (they live in the separate aiukgraphics
// subsystem), so this is the one renderer whose data can't come from
// extract_images; zxextract dumps the irmak globals instead. irmak.c is
// unity-included here to reach DrawPictureNumber + its file-statics; we supply
// the plotting sinks (PutPixel / RectFill) and link palette.c for Remap/pal.
//
// A golden dir holds: tiles.bin, pic<NNN>.dat, and render.txt (one
// "key value" per line: version, palette, picfile, width, height, xoff, yoff).
//
//   usage:
//     zxtest dump <dir>                 ASCII-art the rendered picture
//     zxtest grid <dir> <out.grid>      write the raw RGB grid (C64R1)
//     zxtest cmp  <dir> <golden>        render + byte-compare to golden (C64C2
//                                       .zx, or SCREEN$ .scr)

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "irmak.h"
#include "palette.h"

#define ZX_W 256
#define ZX_H 96

// irmak.c plots palette *indices* (already Remap'd, 0..15); pal[] (palette.c)
// holds their RGB. Capture the displayed RGB, exactly like c64a8test.
#define RG_W ZX_W
#define RG_H ZX_H
#define RG_LABEL "zx"
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

// irmak.c externs we must satisfy.
int last_image_index = 0;

// Unity-include the renderer to reach DrawPictureNumber + the file-static
// PerformTileTransformations / DecodeAttributes / DrawDecodedImage etc.
#include "irmak.c"

// ---- fixture loading ---------------------------------------------------------
// Build the single-image irmak state from a golden dir and render picture 0.
static int render_dir(const char *dir) {
    rg_clear();

    char path[1200], picfile[256] = "";
    int version = 3, palette = 2, width = 32, height = 12, xoff = 0, yoff = 0;

    const rg_kv keys[] = { { "version", &version }, { "palette", &palette }, { "width", &width },
                           { "height", &height }, { "xoff", &xoff }, { "yoff", &yoff }, { NULL, NULL } };
    if (rg_read_render_txt(dir, keys, picfile, sizeof picfile)) return 1;

    // tile font
    size_t tsz = 0;
    snprintf(path, sizeof path, "%s/tiles.bin", dir);
    uint8_t *tilebuf = read_file_ex(path, &tsz, 1);
    if (!tilebuf || tsz < 256 * 8) { fprintf(stderr, "bad tiles.bin\n"); return 1; }
    memcpy(tiles, tilebuf, 256 * 8);
    free(tilebuf);

    // picture data
    size_t psz = 0;
    snprintf(path, sizeof path, "%s/%s", dir, picfile);
    uint8_t *picbuf = read_file_ex(path, &psz, 1);
    if (!picbuf) { fprintf(stderr, "cannot read pic %s\n", path); return 1; }

    // palette: DefinePalette reads palchosen
    palchosen = (palette_type)palette;
    DefinePalette();

    static Image one;
    one.imagedata = picbuf;
    one.datasize = psz;
    one.width = (uint8_t)width;
    one.height = (uint8_t)height;
    one.xoff = (uint8_t)xoff;
    one.yoff = (uint8_t)yoff;
    images = &one;
    InitIrmak(1, version);

    DrawPictureNumber(0, 0);
    return 0;
}

// Compare with a SCREEN$ dump: 6144 bytes of bitmap, whose lines are stored
// with the three bit fields of y shuffled, then 768 attribute bytes.
static int compare_scr(const uint8_t *scr) {
    int match = 0, total = 0, firstx = -1, firsty = -1;
    for (int y = 0; y < ZX_H; y++)
        for (int x = 0; x < ZX_W; x++) {
            if (grid[y][x] == UNSET) continue;
            total++;
            int colour = zx_scr_colour(scr, x, y);
            if (grid[y][x] == (pal[colour] & 0xffffff)) match++;
            else if (firstx < 0) { firstx = x; firsty = y; }
        }
    return rg_report(match, total, firstx, firsty, 0);
}

// A SCREEN$ dump (.scr), or a C64C2 golden.
static int zx_compare_golden(const char *goldenpath) {
    size_t sz = 0;
    uint8_t *gold = read_file_ex(goldenpath, &sz, 1);
    const char *ext = strrchr(goldenpath, '.');
    if (gold && sz == 6912 && ext && strcmp(ext, ".scr") == 0) {
        int result = compare_scr(gold);
        free(gold);
        return result;
    }
    return rg_compare_c64c2(gold, sz, goldenpath);
}

int main(int argc, char **argv) {
    return rg_main(argc, argv, render_dir, zx_compare_golden);
}

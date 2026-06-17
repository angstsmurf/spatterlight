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
#define UNSET 0xffffffffu

static uint32_t grid[ZX_H][ZX_W];
static int grid_minx = ZX_W, grid_miny = ZX_H, grid_maxx = -1, grid_maxy = -1;

static void plot(int x, int y, int color) {
    if (x < 0 || x >= ZX_W || y < 0 || y >= ZX_H) return;
    grid[y][x] = pal[color & 15] & 0xffffff;
    if (x < grid_minx) grid_minx = x;
    if (x > grid_maxx) grid_maxx = x;
    if (y < grid_miny) grid_miny = y;
    if (y > grid_maxy) grid_maxy = y;
}
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

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n ? (size_t)n : 1);
    if (buf && n && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf && size) *size = (size_t)n;
    return buf;
}

static int render_dir(const char *dir) {
    memset(grid, 0xff, sizeof grid);
    grid_minx = ZX_W; grid_miny = ZX_H; grid_maxx = -1; grid_maxy = -1;

    char path[1200], picfile[256] = "";
    int palette = 2, bg = 0;
    snprintf(path, sizeof path, "%s/render.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    char key[64], sval[256]; long val; char line[512];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%63s", key) != 1) continue;
        if      (strcmp(key, "palette") == 0) { sscanf(line, "%*s %ld", &val); palette = (int)val; }
        else if (strcmp(key, "bg")      == 0) { sscanf(line, "%*s %ld", &val); bg = (int)val; }
        else if (strcmp(key, "picfile") == 0) { sscanf(line, "%*s %255s", sval); strncpy(picfile, sval, sizeof picfile - 1); }
    }
    fclose(f);

    size_t psz = 0;
    snprintf(path, sizeof path, "%s/%s", dir, picfile);
    uint8_t *picbuf = read_file(path, &psz);
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

static void dump_ascii(void) {
    if (grid_maxx < 0) { printf("(empty)\n"); return; }
    printf("bbox x[%d..%d] y[%d..%d]\n", grid_minx, grid_maxx, grid_miny, grid_maxy);
    const char *shades = " .:-=+*#%@";
    for (int y = grid_miny; y <= grid_maxy; y += 2) {
        for (int x = grid_minx; x <= grid_maxx; x += 2) {
            uint32_t c = grid[y][x];
            if (c == UNSET) { putchar('?'); continue; }
            int luma = ((c >> 16 & 255) * 30 + (c >> 8 & 255) * 59 + (c & 255) * 11) / 100;
            putchar(shades[luma * 9 / 255]);
        }
        putchar('\n');
    }
}

static void write_grid(const char *out) {
    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); exit(1); }
    int32_t w = ZX_W, h = ZX_H;
    fwrite("C64R1", 1, 5, f);
    fwrite(&w, sizeof w, 1, f); fwrite(&h, sizeof h, 1, f);
    for (int y = 0; y < ZX_H; y++) fwrite(grid[y], sizeof(uint32_t), ZX_W, f);
    fclose(f);
    printf("wrote %s (%dx%d)\n", out, w, h);
}

static int compare_golden(const char *goldenpath) {
    size_t sz = 0;
    uint8_t *gold = read_file(goldenpath, &sz);
    if (!gold || sz < 17 || memcmp(gold, "C64C2", 5) != 0) { fprintf(stderr, "bad golden\n"); return 2; }
    int32_t gw, gh, npal;
    memcpy(&gw, gold + 5, 4); memcpy(&gh, gold + 9, 4); memcpy(&npal, gold + 13, 4);
    const uint8_t *gpal = gold + 17;
    const uint8_t *gidx = gpal + npal * 3;
    #define GOLD_RGB(i) ((uint32_t)gpal[(i)*3] << 16 | (uint32_t)gpal[(i)*3+1] << 8 | gpal[(i)*3+2])
    int match = 0, total = 0, firstx = -1, firsty = -1;
    for (int y = 0; y < ZX_H && y < gh; y++)
        for (int x = 0; x < ZX_W && x < gw; x++) {
            if (grid[y][x] == UNSET) continue;
            total++;
            uint8_t gi = gidx[y * gw + x];
            uint32_t want = (gi == 0xff) ? UNSET : GOLD_RGB(gi);
            if (grid[y][x] == want) match++;
            else if (firstx < 0) { firstx = x; firsty = y; }
        }
    free(gold);
    int ok = (total > 0 && match == total);
    printf("zxvec  %-12s %6d/%6d px match (%.2f%%)%s\n", "render",
           match, total, 100.0 * match / (total ? total : 1), ok ? "  PASS" : "  FAIL");
    if (!ok && firstx >= 0)
        fprintf(stderr, "  first diff at (%d,%d): got #%06x\n", firstx, firsty, grid[firsty][firstx]);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dump|grid|cmp <dir> [out|golden]\n", argv[0]); return 2; }
    const char *mode = argv[1];
    if (render_dir(argv[2]) != 0) return 2;
    if (strcmp(mode, "dump") == 0) { dump_ascii(); return 0; }
    if (strcmp(mode, "grid") == 0) { if (argc < 4) return 2; write_grid(argv[3]); return 0; }
    if (strcmp(mode, "cmp")  == 0) { if (argc < 4) return 2; return compare_golden(argv[3]); }
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}

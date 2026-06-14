// C-renderer side of the DOS SAGA bitmap test. Feeds a raw <name>.PAK image
// (exactly the bytes the interpreter's FindImageFile() hands to DrawDOSImage)
// to common_sagadraw/pcdraw.c:DrawDOSImageFromData, capturing every
// PutPixelWithWidth() into a CGA-resolution slot grid (one 2-bit colour index,
// 0..3, per native pixel). That grid is byte-compared against a golden grid
// decoded from a real DOSBox CGA screenshot of the same room (see
// dos_decode_png.py). Comparison is at the palette-independent slot level, so a
// wrong decode fails regardless of how slots map to RGB.
//
// pcdraw subtracts a constant 24px (the Spatterlight graphics-window left margin)
// from every image's left origin, whereas the real DOS game draws straight into
// the CGA framebuffer; verified empirically that a correct render matches the
// DOSBox golden exactly at dx=+24, dy=0 (Spider-Man R002 = 100%). So the compare
// shifts the C grid right by CGA_DX to line up with the screen-space golden.
//
//   usage:
//     dostest dump  <image.PAK>                 ASCII-art the rendered grid
//     dostest grid  <image.PAK> <out.grid>      write the raw slot grid
//     dostest cmp   <image.PAK> <golden.cga>    render + byte-compare to golden

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"

int DrawDOSImageFromData(uint8_t *ptr);

// ---- CGA logical-pixel capture ------------------------------------------------
#define CGA_W 320
#define CGA_H 200

static int8_t grid[CGA_H][CGA_W];   // -1 = untouched, else slot 0..3
static int grid_minx = CGA_W, grid_miny = CGA_H, grid_maxx = -1, grid_maxy = -1;

static void plot(int x, int y, int color) {
    if (x < 0 || x >= CGA_W || y < 0 || y >= CGA_H)
        return;
    grid[y][x] = (int8_t)(color & 3);
    if (x < grid_minx) grid_minx = x;
    if (x > grid_maxx) grid_maxx = x;
    if (y < grid_miny) grid_miny = y;
    if (y > grid_maxy) grid_maxy = y;
}

// pcdraw draws via PutPixelWithWidth; width 2 means each logical pixel covers two
// native columns (the non-striped, half-horizontal-resolution images).
void PutPixelWithWidth(glsi32 x, glsi32 y, int32_t color, int pixelwidth) {
    for (int i = 0; i < pixelwidth; i++)
        plot((int)x + i, (int)y, (int)color);
}
void PutPixel(glsi32 x, glsi32 y, int32_t color) { plot((int)x, (int)y, (int)color); }
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color) { PutPixelWithWidth(x, y, color, 2); }
void SetColor(int32_t index, glui32 color) { (void)index; (void)color; }

// ---- helpers ------------------------------------------------------------------
static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf && size) *size = (size_t)n;
    return buf;
}

static void render(const char *pakpath) {
    memset(grid, -1, sizeof grid);
    size_t sz = 0;
    uint8_t *pak = read_file(pakpath, &sz);
    if (!pak) { fprintf(stderr, "cannot read %s\n", pakpath); exit(1); }
    DrawDOSImageFromData(pak);
    free(pak);
}

static void dump_ascii(void) {
    const char *shades = " .:#";
    if (grid_maxx < 0) { printf("(empty render)\n"); return; }
    printf("rendered bbox x[%d..%d] y[%d..%d]\n", grid_minx, grid_maxx, grid_miny, grid_maxy);
    for (int y = grid_miny; y <= grid_maxy; y += 2) {        // y+=2: CGA field interlace
        for (int x = grid_minx; x <= grid_maxx; x += 2) {
            int v = grid[y][x];
            putchar(v < 0 ? '?' : shades[v]);
        }
        putchar('\n');
    }
}

// Raw grid file: "DGRID" magic, w, h (int32 LE), then h*w bytes (0..3, 0xff=unset).
static void write_grid(const char *out) {
    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); exit(1); }
    int32_t w = CGA_W, h = CGA_H;
    fwrite("DGRID", 1, 5, f);
    fwrite(&w, sizeof w, 1, f);
    fwrite(&h, sizeof h, 1, f);
    for (int y = 0; y < CGA_H; y++)
        for (int x = 0; x < CGA_W; x++) {
            uint8_t b = grid[y][x] < 0 ? 0xff : (uint8_t)grid[y][x];
            fwrite(&b, 1, 1, f);
        }
    fclose(f);
    printf("wrote %s (%dx%d)\n", out, w, h);
}

// Constant screen-space offset between pcdraw's window coords and the CGA golden.
#define CGA_DX 24

// Golden grid produced by dos_decode_png.py: "CGA00", w,h (int32 LE), w*h bytes.
// Returns 0 (PASS) when every pixel the C renderer drew matches the golden.
static int compare_golden(const char *pakpath, const char *goldenpath) {
    (void)pakpath;  // already rendered into `grid` by render()
    size_t sz = 0;
    uint8_t *gold = read_file(goldenpath, &sz);
    if (!gold || sz < 13 || memcmp(gold, "CGA00", 5) != 0) {
        fprintf(stderr, "bad golden %s\n", goldenpath); return 2;
    }
    int32_t gw, gh;
    memcpy(&gw, gold + 5, 4);
    memcpy(&gh, gold + 9, 4);
    const uint8_t *gpix = gold + 13;

    int match = 0, total = 0, firstx = -1, firsty = -1;
    for (int y = 0; y < CGA_H; y++) {
        for (int x = 0; x < CGA_W; x++) {
            if (grid[y][x] < 0) continue;
            int gx = x + CGA_DX, gy = y;
            if (gx < 0 || gx >= gw || gy < 0 || gy >= gh) continue;
            total++;
            if (grid[y][x] == (int8_t)gpix[gy * gw + gx]) {
                match++;
            } else if (firstx < 0) {
                firstx = x; firsty = y;
            }
        }
    }
    free(gold);
    int ok = (total > 0 && match == total);
    printf("%-12s %5d/%5d pixels match (%.2f%%)%s\n",
           "render", match, total, 100.0 * match / (total ? total : 1),
           ok ? "  PASS" : "  FAIL");
    if (!ok && firstx >= 0)
        fprintf(stderr, "  first diff at C(%d,%d) -> golden(%d,%d): got %d want %d\n",
                firstx, firsty, firstx + CGA_DX, firsty,
                grid[firsty][firstx], gpix[firsty * gw + firstx + CGA_DX]);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s dump|grid|cmp <image.PAK> [out|golden]\n", argv[0]);
        return 2;
    }
    const char *mode = argv[1];
    render(argv[2]);

    if (strcmp(mode, "dump") == 0) {
        dump_ascii();
        return 0;
    }
    if (strcmp(mode, "grid") == 0) {
        if (argc < 4) { fprintf(stderr, "grid needs <out>\n"); return 2; }
        write_grid(argv[3]);
        return 0;
    }
    if (strcmp(mode, "cmp") == 0) {
        if (argc < 4) { fprintf(stderr, "cmp needs <golden.cga>\n"); return 2; }
        return compare_golden(argv[2], argv[3]);
    }
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}

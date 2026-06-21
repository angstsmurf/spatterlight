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

// ---- displayed-colour capture ------------------------------------------------
#define C64_W 320
#define C64_H 200
#define UNSET 0xffffffffu

static uint32_t grid[C64_H][C64_W];      // UNSET, or 0x00RRGGBB displayed colour
static int grid_minx = C64_W, grid_miny = C64_H, grid_maxx = -1, grid_maxy = -1;

static uint32_t slot_rgb[8];             // SetColor sinks: 0..3 bitmap, 5 sprite

static void plot(int x, int y, int slot) {
    if (x < 0 || x >= C64_W || y < 0 || y >= C64_H)
        return;
    grid[y][x] = slot_rgb[slot & 7] & 0xffffff;
    if (x < grid_minx) grid_minx = x;
    if (x > grid_maxx) grid_maxx = x;
    if (y < grid_miny) grid_miny = y;
    if (y > grid_maxy) grid_maxy = y;
}

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

// Parse a render spec and composite it into `grid`. Returns 0 on success.
static int render_spec(const char *specpath) {
    memset(grid, 0xff, sizeof grid);     // UNSET
    memset(slot_rgb, 0, sizeof slot_rgb);

    char dir[1024];
    snprintf(dir, sizeof dir, "%s", specpath);
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0; else dir[0] = 0;

    FILE *f = fopen(specpath, "r");
    if (!f) { fprintf(stderr, "cannot read spec %s\n", specpath); return 1; }

    char line[1024];
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        // strip comments / trailing whitespace
        char *h = strchr(line, '#'); if (h) *h = 0;
        size_t n = strlen(line);
        while (n && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ' || line[n-1] == '\t'))
            line[--n] = 0;
        char *p = line; while (*p == ' ' || *p == '\t') p++;
        if (!*p) continue;

        char path[2048];
        int objindex = -1, isobj = 0;
        // "<index> <file>" => object; bare "<file>" => background (first line)
        char *sp = p;
        while (*sp && *sp != ' ' && *sp != '\t') sp++;
        if (*sp) {  // two tokens -> object line
            *sp = 0; objindex = atoi(p); isobj = 1;
            char *file = sp + 1; while (*file == ' ' || *file == '\t') file++;
            snprintf(path, sizeof path, "%s%s", dir, file);
        } else {
            snprintf(path, sizeof path, "%s%s", dir, p);
        }

        size_t sz = 0;
        uint8_t *blob = read_file(path, &sz);
        if (!blob) { fprintf(stderr, "spec line %d: cannot read %s\n", lineno + 1, path); fclose(f); return 1; }
        if (isobj) render_object(objindex, blob, sz);
        else       DrawMiniC64ImageFromData(blob, sz);
        free(blob);
        lineno++;
    }
    fclose(f);
    return 0;
}

static void dump_ascii(void) {
    if (grid_maxx < 0) { printf("(empty render)\n"); return; }
    printf("rendered bbox x[%d..%d] y[%d..%d]\n", grid_minx, grid_maxx, grid_miny, grid_maxy);
    const char *shades = " .:-=+*#%@";
    for (int y = grid_miny; y <= grid_maxy; y += 2) {
        for (int x = grid_minx; x <= grid_maxx; x += 4) {   // x+=4: double-width pixels
            uint32_t c = grid[y][x];
            if (c == UNSET) { putchar('?'); continue; }
            int luma = ((c >> 16 & 255) * 30 + (c >> 8 & 255) * 59 + (c & 255) * 11) / 100;
            putchar(shades[luma * 9 / 255]);
        }
        putchar('\n');
    }
}

static void print_palette(void) {
    uint32_t seen[64]; int ns = 0;
    for (int y = 0; y < C64_H; y++)
        for (int x = 0; x < C64_W; x++) {
            uint32_t c = grid[y][x];
            if (c == UNSET) continue;
            int found = 0;
            for (int i = 0; i < ns; i++) if (seen[i] == c) { found = 1; break; }
            if (!found && ns < 64) seen[ns++] = c;
        }
    for (int i = 0; i < ns; i++) printf("  #%06x\n", seen[i]);
}

// Grid file: "C64R1" magic, w, h (int32 LE), then h*w uint32 LE (UNSET or RGB).
static void write_grid(const char *out) {
    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); exit(1); }
    int32_t w = C64_W, h = C64_H;
    fwrite("C64R1", 1, 5, f);
    fwrite(&w, sizeof w, 1, f); fwrite(&h, sizeof h, 1, f);
    for (int y = 0; y < C64_H; y++) fwrite(grid[y], sizeof(uint32_t), C64_W, f);
    fclose(f);
    printf("wrote %s (%dx%d)\n", out, w, h);
}

// Golden produced by c64_decode_png.py: "C64C2", w, h, npal (int32 LE), npal*3
// RGB bytes, then w*h index bytes (0xff = UNSET). Each VICE pixel was classified
// to the nearest renderer colour and stored as an index into the renderer's own
// palette, so the compare is an exact RGB-equality check over the drawn pixels.
static int compare_golden(const char *goldenpath) {
    size_t sz = 0;
    uint8_t *gold = read_file(goldenpath, &sz);
    if (!gold || sz < 17 || memcmp(gold, "C64C2", 5) != 0) {
        fprintf(stderr, "bad golden %s\n", goldenpath); return 2;
    }
    int32_t gw, gh, npal;
    memcpy(&gw, gold + 5, 4); memcpy(&gh, gold + 9, 4); memcpy(&npal, gold + 13, 4);
    const uint8_t *gpal = gold + 17;
    const uint8_t *gidx = gpal + npal * 3;
    #define GOLD_RGB(i) ((uint32_t)gpal[(i)*3] << 16 | (uint32_t)gpal[(i)*3+1] << 8 | gpal[(i)*3+2])

    int match = 0, total = 0, firstx = -1, firsty = -1;
    for (int y = 0; y < C64_H && y < gh; y++) {
        for (int x = 0; x < C64_W && x < gw; x++) {
            if (grid[y][x] == UNSET) continue;
            total++;
            uint8_t gi = gidx[y * gw + x];
            uint32_t want = (gi == 0xff) ? UNSET : GOLD_RGB(gi);
            if (grid[y][x] == want) match++;
            else if (firstx < 0) { firstx = x; firsty = y; }
        }
    }
    uint32_t firstwant = 0;
    if (firstx >= 0) {
        uint8_t gi = gidx[firsty * gw + firstx];
        firstwant = (gi == 0xff) ? UNSET : GOLD_RGB(gi);
    }
    free(gold);
    int ok = (total > 0 && match == total);
    printf("c64    %-12s %6d/%6d px match (%.2f%%)%s\n", "render",
           match, total, 100.0 * match / (total ? total : 1), ok ? "  PASS" : "  FAIL");
    if (!ok && firstx >= 0)
        fprintf(stderr, "  first diff at (%d,%d): got #%06x want #%06x\n",
                firstx, firsty, grid[firsty][firstx], firstwant & 0xffffff);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s dump|pal|grid|cmp <spec> [out|golden]\n", argv[0]);
        return 2;
    }
    const char *mode = argv[1];
    if (render_spec(argv[2]) != 0) return 2;

    if (strcmp(mode, "dump") == 0) { dump_ascii(); return 0; }
    if (strcmp(mode, "pal") == 0)  { print_palette(); return 0; }
    if (strcmp(mode, "grid") == 0) {
        if (argc < 4) { fprintf(stderr, "grid needs <out>\n"); return 2; }
        write_grid(argv[3]); return 0;
    }
    if (strcmp(mode, "cmp") == 0) {
        if (argc < 4) { fprintf(stderr, "cmp needs <golden.c64>\n"); return 2; }
        return compare_golden(argv[3]);
    }
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}

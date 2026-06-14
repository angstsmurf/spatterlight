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
#define UNSET 0xffffffffu

static uint32_t grid[C64_H][C64_W];      // UNSET, or 0x00RRGGBB displayed colour
static int grid_minx = C64_W, grid_miny = C64_H, grid_maxx = -1, grid_maxy = -1;

static uint32_t slot_rgb[16];            // SetColor sinks (palette slots 0..4)

// The original C64 display hides the top/bottom picture rows that fall in the
// PAL overscan border (Spider-Man's room images are drawn 2px into the top
// border, so their top two rows show as black, not as picture). Those rows are
// not part of the visible image, so a spec can mark them clipped (!cliptop N /
// !clipbottom N); we then never plot them and they drop out of the comparison.
static int g_cliptop = 0, g_clipbottom = 0;

static void plot(int x, int y, int slot) {
    if (x < 0 || x >= C64_W || y < 0 || y >= C64_H)
        return;
    if (y < g_cliptop || y >= C64_H - g_clipbottom)
        return;
    grid[y][x] = slot_rgb[slot & 15] & 0xffffff;
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

static int g_is_c64 = 1;
static int g_voodoo = 0;

// Parse a render spec and composite it into `grid`. Returns 0 on success.
static int render_spec(const char *specpath) {
    memset(grid, 0xff, sizeof grid);     // UNSET
    memset(slot_rgb, 0, sizeof slot_rgb);
    g_is_c64 = 1; g_voodoo = 0; g_cliptop = 0; g_clipbottom = 0;

    char dir[1024];
    snprintf(dir, sizeof dir, "%s", specpath);
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0; else dir[0] = 0;

    FILE *f = fopen(specpath, "r");
    if (!f) { fprintf(stderr, "cannot read spec %s\n", specpath); return 1; }

    char line[1024];
    int lineno = 0, drew = 0;
    while (fgets(line, sizeof line, f)) {
        // strip comments / trailing whitespace
        char *h = strchr(line, '#'); if (h) *h = 0;
        size_t n = strlen(line);
        while (n && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ' || line[n-1] == '\t'))
            line[--n] = 0;
        char *p = line; while (*p == ' ' || *p == '\t') p++;
        if (!*p) continue;

        // directives
        if (*p == '!') {
            if (strcmp(p, "!atari8") == 0) g_is_c64 = 0;
            else if (strcmp(p, "!voodoo") == 0) g_voodoo = 1;
            else if (strncmp(p, "!cliptop ", 9) == 0) g_cliptop = atoi(p + 9);
            else if (strncmp(p, "!clipbottom ", 12) == 0) g_clipbottom = atoi(p + 12);
            else { fprintf(stderr, "spec line %d: unknown directive %s\n", lineno + 1, p); fclose(f); return 1; }
            lineno++;
            continue;
        }

        char path[2048];
        int isobj = 0;
        // "<index> <file>" => object overlay; bare "<file>" => background
        char *sp = p;
        while (*sp && *sp != ' ' && *sp != '\t') sp++;
        if (*sp) {  // two tokens -> object line
            *sp = 0; isobj = 1;
            char *file = sp + 1; while (*file == ' ' || *file == '\t') file++;
            snprintf(path, sizeof path, "%s%s", dir, file);
        } else {
            snprintf(path, sizeof path, "%s%s", dir, p);
        }

        size_t sz = 0;
        uint8_t *blob = read_file(path, &sz);
        if (!blob) { fprintf(stderr, "spec line %d: cannot read %s\n", lineno + 1, path); fclose(f); return 1; }
        g_is_room = !isobj;
        DrawC64A8ImageFromData(blob, sz, g_voodoo, adjust_room, g_is_c64);
        free(blob);
        drew++;
        lineno++;
    }
    fclose(f);
    if (!drew) { fprintf(stderr, "spec %s drew nothing\n", specpath); return 1; }
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
// RGB bytes, then w*h index bytes (0xff = UNSET). Compare is exact RGB-equality.
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
    printf("c64a8  %-12s %6d/%6d px match (%.2f%%)%s\n", "render",
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

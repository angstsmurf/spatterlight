// The displayed-colour grid shared by the C64 / Atari 8-bit / ZX renderer
// harnesses (c64test, c64a8test, zxtest, zxvectortest). Header only, because
// each harness is one translation unit that unity-includes its renderer.
//
// Every plotted pixel is captured as the colour it is displayed in (0x00RRGGBB)
// in a RG_W x RG_H grid, UNSET where nothing was plotted. The grid can be
// dumped as ASCII art, written raw (C64R1) for the screenshot decoders
// (c64_decode_png.py) and compared with a golden in the renderer's own colour
// space (C64C2), so a compare is a pure RGB-equality check over the plotted
// pixels.
//
// Define before including:
//   RG_W, RG_H          grid size
//   RG_LABEL            first word of the compare line ("c64", "zx", ...)
//   RG_SLOT_RGB(slot)   the displayed colour of a plotted slot / palette index
// Optional:
//   RG_CLIP(x, y)       nonzero to drop a pixel that is inside the grid
//   RG_DUMP_XSTEP       column step of the ASCII dump (default 2; 4 for the
//                       double-width C64 pixels)
//   RG_C64_STYLE        1 for the C64 harnesses' wording: a `pal` mode, named
//                       errors for missing arguments and bad goldens, the
//                       golden colour in the first-difference line, and a
//                       "rendered bbox" / "(empty render)" dump header
//   RG_READ_ALLOW_EMPTY 1 to read a 0-byte golden as empty rather than missing

#ifndef SAGA_TEST_RGBGRID_H
#define SAGA_TEST_RGBGRID_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_util.h"

#ifndef RG_DUMP_XSTEP
#define RG_DUMP_XSTEP 2
#endif
#ifndef RG_C64_STYLE
#define RG_C64_STYLE 0
#endif
#ifndef RG_READ_ALLOW_EMPTY
#define RG_READ_ALLOW_EMPTY 0
#endif
#ifndef RG_CLIP
#define RG_CLIP(x, y) 0
#endif

#define UNSET 0xffffffffu

static uint32_t grid[RG_H][RG_W];        // UNSET, or 0x00RRGGBB displayed colour
static int grid_minx = RG_W, grid_miny = RG_H, grid_maxx = -1, grid_maxy = -1;

// Forget everything plotted.
static void rg_clear(void) {
    memset(grid, 0xff, sizeof grid);
    grid_minx = RG_W; grid_miny = RG_H; grid_maxx = -1; grid_maxy = -1;
}

static void plot(int x, int y, int slot) {
    if (x < 0 || x >= RG_W || y < 0 || y >= RG_H)
        return;
    if (RG_CLIP(x, y))
        return;
    grid[y][x] = RG_SLOT_RGB(slot) & 0xffffff;
    if (x < grid_minx) grid_minx = x;
    if (x > grid_maxx) grid_maxx = x;
    if (y < grid_miny) grid_miny = y;
    if (y > grid_maxy) grid_maxy = y;
}

static void dump_ascii(void) {
    if (grid_maxx < 0) { printf(RG_C64_STYLE ? "(empty render)\n" : "(empty)\n"); return; }
    printf(RG_C64_STYLE ? "rendered bbox x[%d..%d] y[%d..%d]\n" : "bbox x[%d..%d] y[%d..%d]\n",
           grid_minx, grid_maxx, grid_miny, grid_maxy);
    const char *shades = " .:-=+*#%@";
    for (int y = grid_miny; y <= grid_maxy; y += 2) {
        for (int x = grid_minx; x <= grid_maxx; x += RG_DUMP_XSTEP) {
            uint32_t c = grid[y][x];
            if (c == UNSET) { putchar('?'); continue; }
            int luma = ((c >> 16 & 255) * 30 + (c >> 8 & 255) * 59 + (c & 255) * 11) / 100;
            putchar(shades[luma * 9 / 255]);
        }
        putchar('\n');
    }
}

// The distinct displayed colours, in order of first appearance (at most 64).
static void print_palette(void) {
    uint32_t seen[64]; int ns = 0;
    for (int y = 0; y < RG_H; y++)
        for (int x = 0; x < RG_W; x++) {
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
    int32_t w = RG_W, h = RG_H;
    fwrite("C64R1", 1, 5, f);
    fwrite(&w, sizeof w, 1, f); fwrite(&h, sizeof h, 1, f);
    for (int y = 0; y < RG_H; y++) fwrite(grid[y], sizeof(uint32_t), RG_W, f);
    fclose(f);
    printf("wrote %s (%dx%d)\n", out, w, h);
}

// Print the result line of a compare (and, on failure, where the first
// difference is); returns the exit code: 0 pass, 1 fail.
static int rg_report(int match, int total, int firstx, int firsty, uint32_t firstwant) {
    int ok = (total > 0 && match == total);
    printf("%-6s %-12s %6d/%6d px match (%.2f%%)%s\n", RG_LABEL, "render",
           match, total, 100.0 * match / (total ? total : 1), ok ? "  PASS" : "  FAIL");
    if (!ok && firstx >= 0) {
        if (RG_C64_STYLE)
            fprintf(stderr, "  first diff at (%d,%d): got #%06x want #%06x\n",
                    firstx, firsty, grid[firsty][firstx], firstwant & 0xffffff);
        else
            fprintf(stderr, "  first diff at (%d,%d): got #%06x\n", firstx, firsty, grid[firsty][firstx]);
    }
    return ok ? 0 : 1;
}

// Compare with a golden produced by the decoders: "C64C2", w, h, npal (int32
// LE), npal*3 RGB bytes, then w*h index bytes (0xff = UNSET). Each screenshot
// pixel was classified to the nearest renderer colour and stored as an index
// into the renderer's own palette, so the compare is an exact RGB-equality
// check over the drawn pixels. Takes ownership of gold.
static int rg_compare_c64c2(uint8_t *gold, size_t sz, const char *goldenpath) {
    if (!gold || sz < 17 || memcmp(gold, "C64C2", 5) != 0) {
        if (RG_C64_STYLE) fprintf(stderr, "bad golden %s\n", goldenpath);
        else fprintf(stderr, "bad golden\n");
        free(gold);
        return 2;
    }
    int32_t gw, gh, npal;
    memcpy(&gw, gold + 5, 4); memcpy(&gh, gold + 9, 4); memcpy(&npal, gold + 13, 4);
    const uint8_t *gpal = gold + 17;
    const uint8_t *gidx = gpal + npal * 3;
    #define GOLD_RGB(i) ((uint32_t)gpal[(i)*3] << 16 | (uint32_t)gpal[(i)*3+1] << 8 | gpal[(i)*3+2])

    int match = 0, total = 0, firstx = -1, firsty = -1;
    for (int y = 0; y < RG_H && y < gh; y++) {
        for (int x = 0; x < RG_W && x < gw; x++) {
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
    #undef GOLD_RGB
    free(gold);
    return rg_report(match, total, firstx, firsty, firstwant);
}

static int compare_golden(const char *goldenpath) {
    size_t sz = 0;
    uint8_t *gold = read_file_ex(goldenpath, &sz, RG_READ_ALLOW_EMPTY);
    return rg_compare_c64c2(gold, sz, goldenpath);
}

// `<harness> dump|[pal|]grid|cmp <input> [out|golden]`: render the input with
// render() (nonzero = could not), then act on the grid; compare() does `cmp`.
static int rg_main(int argc, char **argv, int (*render)(const char *),
                   int (*compare)(const char *)) {
    if (argc < 3) {
        if (RG_C64_STYLE)
            fprintf(stderr, "usage: %s dump|pal|grid|cmp <spec> [out|golden]\n", argv[0]);
        else
            fprintf(stderr, "usage: %s dump|grid|cmp <dir> [out|golden]\n", argv[0]);
        return 2;
    }
    const char *mode = argv[1];
    if (render(argv[2]) != 0) return 2;

    if (strcmp(mode, "dump") == 0) { dump_ascii(); return 0; }
    if (RG_C64_STYLE && strcmp(mode, "pal") == 0) { print_palette(); return 0; }
    if (strcmp(mode, "grid") == 0) {
        if (argc < 4) { if (RG_C64_STYLE) fprintf(stderr, "grid needs <out>\n"); return 2; }
        write_grid(argv[3]); return 0;
    }
    if (strcmp(mode, "cmp") == 0) {
        if (argc < 4) { if (RG_C64_STYLE) fprintf(stderr, "cmp needs <golden.c64>\n"); return 2; }
        return compare(argv[3]);
    }
    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}

// ---- C64 / Atari render specs ------------------------------------------------
// A small text file, paths relative to the spec's dir, '#' comments:
//     <background.dat>
//     <objindex> <object.dat>      ... zero or more objects
// on_directive (may be NULL: then there are no directives) gets each line
// starting with '!' and its 0-based line number, and returns nonzero to fail.
// on_image gets each image with its object index (-1 for a background).
// Returns the number of images drawn, or -1 after printing an error.
static int rg_parse_spec(const char *specpath, int (*on_directive)(const char *p, int lineno),
                         void (*on_image)(int isobj, int objindex, uint8_t *blob, size_t sz)) {
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", specpath);
    char *slash = strrchr(dir, '/');
    if (slash) slash[1] = 0; else dir[0] = 0;

    FILE *f = fopen(specpath, "r");
    if (!f) { fprintf(stderr, "cannot read spec %s\n", specpath); return -1; }

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

        if (on_directive && *p == '!') {
            if (on_directive(p, lineno)) { fclose(f); return -1; }
            lineno++;
            continue;
        }

        char path[2048];
        int objindex = -1, isobj = 0;
        // "<index> <file>" => object; bare "<file>" => background
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
        if (!blob) { fprintf(stderr, "spec line %d: cannot read %s\n", lineno + 1, path); fclose(f); return -1; }
        on_image(isobj, objindex, blob, sz);
        free(blob);
        drew++;
        lineno++;
    }
    fclose(f);
    return drew;
}

// ---- ZX golden dirs ----------------------------------------------------------
// <dir>/render.txt holds one "key value" per line. picfile (a file name) goes
// to picfile; the integer keys listed in kv (ended by a NULL key) to their
// variables; other keys are ignored. Returns nonzero if there is no render.txt.
typedef struct { const char *key; int *val; } rg_kv;

static int rg_read_render_txt(const char *dir, const rg_kv *kv, char *picfile, size_t picfile_size) {
    char path[1200];
    snprintf(path, sizeof path, "%s/render.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    char key[64], sval[256]; long val; char line[512];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%63s", key) != 1) continue;
        if (strcmp(key, "picfile") == 0) {
            sscanf(line, "%*s %255s", sval);
            strncpy(picfile, sval, picfile_size - 1);
            continue;
        }
        for (const rg_kv *k = kv; k->key; k++)
            if (strcmp(key, k->key) == 0) { sscanf(line, "%*s %ld", &val); *k->val = (int)val; break; }
    }
    fclose(f);
    return 0;
}

#endif

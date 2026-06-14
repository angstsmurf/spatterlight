// C-renderer side of the Apple II SAGA *bitmap* test (distinct from the already
// tested Apple II vector path). Feeds a raw image blob (extracted by
// extract_images from the real game disk) to common_sagadraw/apple2draw.c:
// DrawApple2ImageFromData, which decodes it into `screenmem` — the native Apple
// II hi-res page-1 layout ($2000..$3FFF). That page is byte-compared against a
// golden $2000 dump captured from the real game under MAME (approach A, exactly
// like the vector apple2 test compares its hi-res page).
//
//   usage:
//     apple2test cmp <image.dat> <golden.page> [descrambletable.bin]
//
// If a descramble table is given it is installed first (the Apple II bitmap
// games store a $2000 row-address LUT on disk; DrawApple2ImageFromData takes the
// scrambled path when it is present).

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "apple2draw.h"   // A2_SCREEN_MEM_SIZE, DrawApple2ImageFromData, screenmem

extern uint8_t *screenmem;
extern uint8_t *descrambletable;

// --- stubs for the Glk plotting / artifact-colour path (never reached here;
//     DrawApple2ImageFromData only writes screenmem) -----------------------------
winid_t Graphics = NULL;
int pixel_size = 1, x_offset = 0, y_offset = 0, ImageHeight = 320;
void Fatal(const char *x) { fprintf(stderr, "Fatal: %s\n", x ? x : ""); exit(1); }
void glk_window_fill_rect(winid_t w, glui32 c, glsi32 l, glsi32 t, glui32 ww, glui32 hh) {
    (void)w;(void)c;(void)l;(void)t;(void)ww;(void)hh;
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b && size) *size = (size_t)n;
    return b;
}

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "cmp") != 0) {
        fprintf(stderr, "usage: %s cmp <image.dat> <golden.page> [descrambletable.bin]\n", argv[0]);
        return 2;
    }
    if (argc >= 5) {
        size_t ds = 0;
        descrambletable = read_file(argv[4], &ds);
        if (!descrambletable) { fprintf(stderr, "cannot read descramble table\n"); return 2; }
    }

    size_t isz = 0;
    uint8_t *img = read_file(argv[2], &isz);
    if (!img) { fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }

    // is_the_count = 0 for Hulk-style games; the count flag is game-specific.
    DrawApple2ImageFromData(img, isz, 0, NULL);
    free(img);

    size_t gsz = 0;
    uint8_t *gold = read_file(argv[3], &gsz);
    if (!gold || gsz < A2_SCREEN_MEM_SIZE) {
        fprintf(stderr, "cannot read golden page (need %d bytes)\n", A2_SCREEN_MEM_SIZE);
        return 2;
    }
    if (!screenmem) { fprintf(stderr, "renderer produced no screenmem\n"); return 1; }

    // Compare only the picture rows (hi-res rows 0..159). Rows 160..191 of page 1
    // hold the 4-line text window, which the game draws but the image renderer
    // leaves blank (see groundtruth/README.md), so they are excluded.
    int diff = 0, total = 0, first = -1;
    for (int y = 0; y < 160; y++) {
        unsigned base = (((y / 8) & 0x07) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10);
        for (int x = 0; x < 40; x++) {
            unsigned i = base + x;
            total++;
            if (screenmem[i] != gold[i]) { if (first < 0) first = (int)i; diff++; }
        }
    }
    free(gold);

    printf("apple2 %-20s %d/%d image bytes match (%.2f%%)%s\n",
           argv[2], total - diff, total, 100.0 * (total - diff) / total,
           diff ? "  FAIL" : "  PASS");
    if (diff) fprintf(stderr, "  first diff at 0x%04x: got 0x%02x want 0x%02x\n",
                      first, screenmem[first], gold[first]);
    return diff ? 1 : 0;
}

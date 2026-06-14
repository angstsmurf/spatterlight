// C-renderer side of the TI-99/4A RTPI (Return to Pirate's Isle) bitmap test.
//
// RTPI images are a GROM bytecode stream that ti994a/rtpi_graphics.c decodes
// into two native VDP buffers: vdp_pixels[] (the 1-bpp pattern data, screen
// order, 32x12 tiles = 0xC00 bytes) and vdp_colors[] (the per-row colour
// attribute, fg<<4|bg, same size); DrawRTPIFromMem then plots those to screen.
//
// The golden is captured as the final DISPLAYED picture (approach B): a raw VDP
// pattern/colour-table compare does NOT work because on real hardware RTPI
// splits the picture across both VDP banks with a mid-screen raster trick, and
// — crucially — the in-game pictures are deliberately DISTORTED (the myopia /
// "no glasses" effect, DrawFuzzyRTPIImage) until the player gets up and wears
// the glasses. That distortion also depends on the *previous* screen buffer, so
// it is not reproducible from a blob alone. With the glasses on, the clean
// DrawRTPIImage output matches the hardware framebuffer pixel-for-pixel.
//
// rtpi_graphics.c keeps vdp_pixels/vdp_colors and the 52-tile font (loaded from
// the cartridge CPU ROM) file-static, so we #include the .c here as a unity
// build to reach them; the Makefile drops rtpi_graphics.c from the link list to
// avoid a duplicate definition. dump mode links the full loader (same object set
// as extract_images) to pull blobs + font from a real cartridge; cmp mode is
// self-contained (font + blob only — the draw path touches no other globals).
//
//   usage:
//     titest dump <game.rpk> <outdir>
//         export font.bin + every image's raw .dat blob (+ .vdp/.ppm previews)
//         for setting up goldens.
//     titest cmp <font.bin> <blob.dat> <usage> <index> <golden.pgrid>
//         render the blob and pixel-compare the displayed picture (256x96
//         palette-index grid) to a hardware golden captured under MAME.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Unity-include the renderer to reach its file-static VDP buffers / tile font.
#include "rtpi_graphics.c"

#include "scott.h"
#include "detect_game.h"

extern const char *game_file;

// TMS9918A palette (mirrors SetupRTPIColors), index 0..15 -> 0xRRGGBB.
static const uint32_t ti_palette[16] = {
    0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5051E0, 0x7F77F8, 0xE15D53, 0x42ebf5,
    0xff655C, 0xff847e, 0xD4C154, 0xEAD087, 0x00ae3f, 0xc95bba, 0xCDCDCD, 0xffffff,
};

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

// Render the current vdp_pixels/vdp_colors to a 256x96 RGB PPM (same tile walk
// as DrawRTPIFromMem: 32 cols x 12 rows, fg = hi nibble, bg = lo nibble).
static void write_ppm(const char *path) {
    enum { W = 256, H = 96 };
    static uint8_t rgb[W * H * 3];
    const uint8_t *ptr = vdp_pixels, *colorptr = vdp_colors;
    int x = 0, y = 0;
    while (y < H && ptr < vdp_pixels + RTPI_IMAGE_SIZE) {
        for (int line = 0; line < 8; line++) {
            uint8_t byte = *ptr++;
            uint8_t attr = *colorptr++;
            uint8_t bg = attr & 0xf, fg = attr >> 4;
            for (int p = 0; p < 8; p++) {
                uint32_t c = ti_palette[(byte & (0x80 >> p)) ? fg : bg];
                int px = x + p, py = y + line;
                if (px < W && py < H) {
                    uint8_t *o = rgb + (py * W + px) * 3;
                    o[0] = c >> 16; o[1] = c >> 8; o[2] = c;
                }
            }
        }
        x += 8;
        if (x >= W) { x = 0; y += 8; }
    }
    FILE *f = fopen(path, "wb");
    if (f) { fprintf(f, "P6\n%d %d\n255\n", W, H); fwrite(rgb, 1, sizeof rgb, f); fclose(f); }
}

// Build the displayed picture as a 256x96 grid of palette indices (one byte per
// pixel) from the decoded vdp_pixels/vdp_colors, matching DrawRTPIFromMem's tile
// walk. Palette entries 0 and 1 are both black on the TMS9918, so 1 is folded to
// 0 — exactly the normalisation ti_decode_fb.py applies to the MAME framebuffer,
// so the two grids are directly comparable (approach B).
#define PIC_W 256
#define PIC_H 96
#define PGRID_SIZE (PIC_W * PIC_H)
static void grid_from_vdp(uint8_t *out) {
    const uint8_t *ptr = vdp_pixels, *colorptr = vdp_colors;
    int x = 0, y = 0;
    while (y < PIC_H && ptr < vdp_pixels + RTPI_IMAGE_SIZE) {
        for (int line = 0; line < 8; line++) {
            uint8_t byte = *ptr++, attr = *colorptr++;
            uint8_t bg = attr & 0xf, fg = attr >> 4;
            for (int p = 0; p < 8; p++) {
                uint8_t idx = (byte & (0x80 >> p)) ? fg : bg;
                out[(y + line) * PIC_W + x + p] = (idx == 1) ? 0 : idx;
            }
        }
        x += 8;
        if (x >= PIC_W) { x = 0; y += 8; }
    }
}

// Render a single RTPI image self-contained: install the 52-tile font (the
// 416 bytes rtpi_graphics.c loads from the cartridge CPU ROM) and decode the raw
// GROM blob via the real DrawRTPIImage. The draw/decode path touches no globals
// beyond rtpi_tile_font and the image, so no DetectGame / .rpk is needed.
static int render_blob(const char *fontpath, const char *blobpath, int usage, int index) {
    size_t fsz = 0, bsz = 0;
    uint8_t *font = read_file(fontpath, &fsz);
    uint8_t *blob = read_file(blobpath, &bsz);
    if (!font || fsz < sizeof rtpi_tile_font) { fprintf(stderr, "bad font %s\n", fontpath); return 2; }
    if (!blob || bsz == 0) { fprintf(stderr, "bad blob %s\n", blobpath); return 2; }
    memcpy(rtpi_tile_font, font, sizeof rtpi_tile_font);
    free(font);

    USImage img = (USImage){0};
    img.imagedata = blob;          // DrawRTPIImage may realloc this (DecodeRTPIColors)
    img.datasize = bsz;
    img.usage = usage;
    img.index = index;
    img.systype = SYS_TI994A;
    memset(vdp_pixels, 0, RTPI_IMAGE_SIZE);
    memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
    DrawRTPIImage(&img);
    free(img.imagedata);
    return 0;
}

// Self-contained byte-exact test: render <blob> with <font>, then compare the
// resulting displayed picture (palette-index grid) to a golden grid captured
// from real TI-99 hardware under MAME (see groundtruth_ti99/README.md).
static int do_cmp(const char *fontpath, const char *blobpath, int usage, int index,
                  const char *goldpath) {
    if (render_blob(fontpath, blobpath, usage, index) != 0) return 2;
    static uint8_t grid[PGRID_SIZE];
    grid_from_vdp(grid);

    size_t gsz = 0;
    uint8_t *gold = read_file(goldpath, &gsz);
    if (!gold || gsz < PGRID_SIZE) { fprintf(stderr, "bad golden %s\n", goldpath); return 2; }

    int diff = 0, first = -1;
    for (int i = 0; i < PGRID_SIZE; i++)
        if (grid[i] != gold[i]) { if (first < 0) first = i; diff++; }
    free(gold);

    printf("ti994a %-28s %d/%d px match (%.2f%%)%s\n",
           blobpath, PGRID_SIZE - diff, PGRID_SIZE, 100.0 * (PGRID_SIZE - diff) / PGRID_SIZE,
           diff ? "  FAIL" : "  PASS");
    if (diff) fprintf(stderr, "  first diff at px (%d,%d): got %d want %d\n",
                      first % PIC_W, first / PIC_W, grid[first], gold[first]);
    return diff ? 1 : 0;
}

static int do_dump(const char *game, const char *outdir) {
    game_file = game;
    GameIDType gt = DetectGame(game);
    fprintf(stderr, "DetectGame -> %d, CurrentSys=%d\n", (int)gt, CurrentSys);

    // The 52-tile font (loaded from the cartridge CPU ROM) is the second input
    // a self-contained cmp needs alongside each raw blob; dump it once.
    char path[1100];
    snprintf(path, sizeof path, "%s/font.bin", outdir);
    FILE *ff = fopen(path, "wb");
    if (ff) { fwrite(rtpi_tile_font, 1, sizeof rtpi_tile_font, ff); fclose(ff); }

    int n = 0;
    for (USImage *img = USImages; img != NULL; img = img->next) {
        if (img->imagedata == NULL || img->datasize == 0) continue;
        if (img->systype != SYS_TI994A) continue;

        // Raw GROM blob, BEFORE DrawRTPIImage mutates imagedata — this is the
        // input a self-contained cmp replays.
        snprintf(path, sizeof path, "%s/ti994a_u%d_i%03d.dat", outdir, img->usage, img->index);
        FILE *f = fopen(path, "wb");
        if (f) { fwrite(img->imagedata, 1, img->datasize, f); fclose(f); }

        memset(vdp_pixels, 0, RTPI_IMAGE_SIZE);
        memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
        DrawRTPIImage(img);

        snprintf(path, sizeof path, "%s/ti994a_u%d_i%03d.vdp", outdir, img->usage, img->index);
        f = fopen(path, "wb");
        if (f) {
            fwrite(vdp_pixels, 1, RTPI_IMAGE_SIZE, f);
            fwrite(vdp_colors, 1, RTPI_IMAGE_SIZE, f);
            fclose(f);
        }
        snprintf(path, sizeof path, "%s/ti994a_u%d_i%03d.ppm", outdir, img->usage, img->index);
        write_ppm(path);
        printf("  u%d i%03d  (%zu bytes in)\n", img->usage, img->index, img->datasize);
        n++;
    }
    printf("rendered %d image(s)\n", n);
    return n ? 0 : 1;
}

// Diagnostic: render a sequence of image indices in order WITHOUT clearing the
// VDP buffers between them, to probe whether RTPI images depend on residual VDP
// state (copy-from-vdp opcodes reading tiles drawn by earlier images). Dumps the
// final image's vdp/ppm to outdir. Indices are matched against img->index for
// usage 0 (room images). Pass them as a comma list, e.g. "99,0,2".
static int do_seq(const char *game, const char *outdir, const char *list) {
    game_file = game;
    DetectGame(game);
    memset(vdp_pixels, 0, RTPI_IMAGE_SIZE);
    memset(vdp_colors, 0, RTPI_IMAGE_SIZE);
    char buf[256]; strncpy(buf, list, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    int last = -1;
    for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
        int want = atoi(tok); last = want;
        for (USImage *img = USImages; img; img = img->next) {
            if (img->systype == SYS_TI994A && img->usage == 0 && img->index == want
                && img->imagedata && img->datasize) {
                DrawRTPIImage(img);
                fprintf(stderr, "  drew i%03d\n", want);
                break;
            }
        }
    }
    char path[1100];
    snprintf(path, sizeof path, "%s/seq_i%03d.vdp", outdir, last);
    FILE *f = fopen(path, "wb");
    if (f) { fwrite(vdp_pixels, 1, RTPI_IMAGE_SIZE, f); fwrite(vdp_colors, 1, RTPI_IMAGE_SIZE, f); fclose(f); }
    snprintf(path, sizeof path, "%s/seq_i%03d.ppm", outdir, last);
    write_ppm(path);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 5 && strcmp(argv[1], "seq") == 0)
        return do_seq(argv[2], argv[3], argv[4]);
    if (argc >= 4 && strcmp(argv[1], "dump") == 0)
        return do_dump(argv[2], argv[3]);
    if (argc >= 7 && strcmp(argv[1], "cmp") == 0)
        return do_cmp(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]), argv[6]);
    fprintf(stderr,
        "usage: %s dump <game.rpk> <outdir>\n"
        "         export font.bin + per-image .dat/.vdp/.ppm for golden setup\n"
        "       %s cmp <font.bin> <blob.dat> <usage> <index> <golden.pgrid>\n"
        "         render the blob and pixel-compare to a hardware golden grid\n",
        argv[0], argv[0]);
    return 2;
}

/* test_gmcgav1_pics.cpp -- regression test for the v1 Comprehend DOS CGA renderer.
 *
 * The v1 releases (Crimson Crown, Transylvania v1) split the Penguin Graphics
 * Magician across NOVEL.EXE (brush bitmaps) and PC_GRAPH.OVR (fill pattern +
 * subindex tables, copied into NOVEL.EXE's DGROUP at overlay load).  This test
 * loads those tables via gmcgaInstallV1DrawingTables() and renders Crimson
 * Crown rooms through graphics_magician_cga.cpp, comparing the 280x160 picture
 * window (at screen origin (20,0)) against byte-exact CGA framebuffers captured
 * from DOSBox-X -- in palette-index space, so the palette intensity choice does
 * not matter.
 *
 * Self-contained: needs neither DOSBox nor the copyrighted game files at test
 * time.  Committed in test/gmcgav1/:
 *   novel_ovr_tables.bin -- the PC_GRAPH.OVR fill pattern (30x4) + subindex
 *                           (109 words) slice, starting at the fill-pattern sig.
 *   novel_brushes.bin    -- the 256-byte NOVEL.EXE brush table (brush 5 sig at
 *                           offset 160, so the loader's signature scan resolves).
 *   <scene>.img          -- the room's raw Graphics Magician vector stream.
 *   <scene>.fb           -- the 16 KB CGA VRAM golden from DOSBox.
 *
 * Build/run:  make -C terps/comprehend test
 *
 * Fixtures: lakeshore (RA pic 0), woods (RA pic 2) and cave (RA pic 6) render
 * pixel-exact (ceiling 0).  crypt (RA pic 3) is the only fixture with two-colour
 * flood-fill dithers and so guards the v1 fill: per-column pattern indexing swaps
 * its cyan/magenta dither (2827 diffs); it has a 3-pixel residual from an op12
 * BRUSH spill-byte phase (the flood fill itself is exact -- see the kCases note).
 * rb_03 (RB pic 3) guards the brush/
 * text/rect pattern-PHASE fix.  Any change must stay within these.
 */

#include "../graphics_magician_cga.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace Glk::Comprehend;

#define PIC_W 280
#define PIC_H 160

static std::vector<uint8_t> readFile(const char *path) {
    std::vector<uint8_t> v;
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return v; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0) { v.resize((size_t)n); if (fread(v.data(), 1, (size_t)n, f) != (size_t)n) v.clear(); }
    fclose(f);
    return v;
}

static std::vector<uint8_t> vramToFb(const std::vector<uint8_t> &vram) {
    std::vector<uint8_t> fb(320 * 200, 0);
    if (vram.size() < 0x4000) return fb;
    for (int y = 0; y < 200; y++) {
        int base = (y & 1) * 0x2000 + (y >> 1) * 80, o = y * 320;
        for (int xb = 0; xb < 80; xb++) {
            uint8_t b = vram[base + xb];
            fb[o] = (b >> 6) & 3; fb[o+1] = (b >> 4) & 3;
            fb[o+2] = (b >> 2) & 3; fb[o+3] = b & 3;
            o += 4;
        }
    }
    return fb;
}

static uint8_t rgbaToIndex(uint32_t c) {
    switch (c) {
    case 0x000000ffu: return 0; case 0x00aaaaffu: return 1;
    case 0xaa00aaffu: return 2; case 0xaaaaaaffu: return 3;
    }
    return 0;
}

int main() {
    std::vector<uint8_t> ovr = readFile("test/gmcgav1/novel_ovr_tables.bin");
    std::vector<uint8_t> exe = readFile("test/gmcgav1/novel_brushes.bin");
    if (ovr.empty() || exe.empty()) {
        fprintf(stderr, "missing v1 table fixtures\n");
        return 1;
    }
    if (!gmcgaInstallV1DrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size())) {
        fprintf(stderr, "gmcgaInstallV1DrawingTables failed\n");
        return 1;
    }

    // `ceil` is the maximum tolerated mismatch -- now 0 for every fixture.  The
    // crypt (RA pic 3) is the only fixture with two-colour dithers, so it guards
    // the v1 period-4 fill fix (per-column indexing balloons it to 2827) AND the
    // brush blitter's painted-byte dither-phase walk: its former 3 residual px at
    // x=188 were an op12 BRUSH spill byte whose dither phase the native advances
    // per painted byte (spill helper 0x94c skips inc DI on a blank byte), not per
    // column -- so the off-grid spill lands on phase 3 (magenta), not phase 0
    // (cyan).  blit_col now replicates that walk.  See TODO_v1_cga_graphics.
    static const struct { const char *name, *img, *fb; int ceil; } kCases[] = {
        { "lakeshore", "test/gmcgav1/lakeshore.img", "test/gmcgav1/lakeshore.fb", 0 },
        { "woods",     "test/gmcgav1/woods.img",     "test/gmcgav1/woods.fb",     0 },
        { "cave",      "test/gmcgav1/cave.img",      "test/gmcgav1/cave.fb",      0 },
        { "crypt",     "test/gmcgav1/crypt.img",     "test/gmcgav1/crypt.fb",     0 },
        // RB pic 3: an op12 brush stamps the [40 80 40 80] dither over a flood
        // fill.  Guards the brush/text/rect fill-pattern PHASE fix -- the per-
        // pixel blitters index the pattern by the GLOBAL CGA byte ((x+20)>>2)&3,
        // not entry[0]; collapsing to entry[0] left 51 px wrong here.  Pixel-
        // exact after the fix (see TODO_v1_cga_graphics "MOSTLY FIXED").
        { "rb_03",     "test/gmcgav1/rb_03.img",     "test/gmcgav1/rb_03.fb",     0 },
        // Transylvania v1 start room (RA pic 0).  Its fill/subindex/brush tables
        // are byte-identical to Crimson Crown's, so the same installed tables
        // render it -- this guards that the v1 path generalises across both games.
        { "tr_start",  "test/gmcgav1/tr_start.img",  "test/gmcgav1/tr_start.fb",  0 },
    };

    int failures = 0;
    for (const auto &c : kCases) {
        std::vector<uint8_t> img = readFile(c.img);
        std::vector<uint8_t> golden = vramToFb(readFile(c.fb));
        if (img.empty() || golden.size() != 320 * 200) {
            printf("%-12s MISSING FIXTURE\n", c.name); failures++; continue;
        }
        gmcgaResetScreen(true);          // white cold page (room change)
        gmcgaDrawImage(img.data(), img.size());
        std::vector<uint32_t> rgba(PIC_W * PIC_H);
        gmcgaBlitToSurface(rgba.data(), PIC_W, PIC_H);

        int mism = 0;
        for (int y = 0; y < PIC_H; y++)
            for (int x = 0; x < PIC_W; x++)
                if (rgbaToIndex(rgba[y * PIC_W + x]) != golden[y * 320 + (x + 20)])
                    mism++;
        bool ok = mism <= c.ceil;
        printf("%-12s %d / %d mismatches (ceiling %d)  %s\n",
               c.name, mism, PIC_W * PIC_H, c.ceil, ok ? "ok" : "FAIL");
        if (!ok) failures++;
    }

    if (failures) { printf("GMCGA v1 picture regression: %d case(s) FAILED\n", failures); return 1; }
    printf("GMCGA v1 picture regression: all cases within ceiling\n");
    return 0;
}

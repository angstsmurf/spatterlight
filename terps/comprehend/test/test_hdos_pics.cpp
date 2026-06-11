/* test_hdos_pics.cpp -- regression test for the DOS Talisman CGA renderer.
 *
 * Each case pairs a picture's raw Graphics-Magician vector stream (extracted
 * from the DOS release of Talisman, NOVEL1.EXE / RA / T0) with a byte-exact
 * ground-truth framebuffer captured from DOSBox-X.  The test renders the stream
 * through hdos_talisman.cpp and compares the 280x160 picture window, at screen
 * origin (20, 0), against the golden in CGA palette-index space (0=black,
 * 1=cyan, 2=magenta, 3=white) -- so the high/low intensity palette choice does
 * not matter.
 *
 * Self-contained: needs neither DOSBox nor the copyrighted NOVEL1.EXE at test
 * time.  The drawing tables (fill / subindex / brush / font) are committed as a
 * small slice of NOVEL1.EXE in test/hdos/novel_tables.bin; each scene's vector
 * stream is test/hdos/<scene>.img; each golden 320x200 framebuffer is
 * test/hdos/<scene>.fb.  See test/hdos/README.md for how the goldens were made.
 *
 * Build/run:  make -C terps/comprehend test
 *
 * The renderer is pixel-exact on every fixture, so every ceiling is 0: the
 * golden set is locked and any renderer change must keep this at 0 diffs.
 */

#include "../hdos_talisman.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace Glk::Comprehend;

// Golden framebuffer geometry: full DOS screen is 320x200, the picture window
// is 280x160 at screen origin (20, 0).
#define SCREEN_W 320
#define PIC_W    280
#define PIC_H    160
#define ORIGIN_X 20
#define ORIGIN_Y 0

struct Case {
    const char *name;
    const char *img;   // vector stream file
    const char *fb;    // golden framebuffer file
    int         ceil;  // max tolerated mismatching pixels (current baseline)
};

static const Case kCases[] = {
    { "title",     "test/hdos/title.img",     "test/hdos/title.fb",     0 },
    { "throne",    "test/hdos/throne.img",    "test/hdos/throne.fb",    0 },
    { "cell",      "test/hdos/cell.img",      "test/hdos/cell.fb",      0 },
    { "courtyard", "test/hdos/courtyard.img", "test/hdos/courtyard.fb", 0 },
};

static std::vector<uint8_t> readFile(const std::string &path) {
    std::vector<uint8_t> v;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        v.resize((size_t)n);
        if (fread(v.data(), 1, (size_t)n, f) != (size_t)n)
            v.clear();
    }
    fclose(f);
    return v;
}

// Map a renderer RGBA pixel (0xRRGGBBAA, kHdosColor) to its CGA palette index.
static int rgbaToIndex(uint32_t p) {
    switch (p >> 8) {  // drop alpha
    case 0x000000: return 0; // black
    case 0x55ffff: return 1; // cyan  (high intensity)
    case 0xff55ff: return 2; // magenta
    case 0xffffff: return 3; // white
    default:       return 0;
    }
}

int main() {
    std::vector<uint8_t> tables = readFile("test/hdos/novel_tables.bin");
    if (tables.empty() || !hdosInstallDrawingTables(tables.data(), tables.size())) {
        fprintf(stderr, "FAIL: cannot load drawing tables from test/hdos/novel_tables.bin\n");
        return 1;
    }

    int failures = 0;
    for (const Case &c : kCases) {
        std::vector<uint8_t> img = readFile(c.img);
        std::vector<uint8_t> fb  = readFile(c.fb);
        if (img.empty()) { fprintf(stderr, "FAIL %s: cannot read %s\n", c.name, c.img); failures++; continue; }
        if (fb.size() != (size_t)SCREEN_W * 200) {
            fprintf(stderr, "FAIL %s: %s wrong size %zu\n", c.name, c.fb, fb.size());
            failures++;
            continue;
        }

        hdosResetScreen(true);          // cold page: solid white
        hdosDrawImage(img.data(), img.size());

        std::vector<uint32_t> rgba((size_t)PIC_W * PIC_H);
        hdosBlitToSurface(rgba.data(), PIC_W, PIC_H);

        int diffs = 0;
        for (int y = 0; y < PIC_H; y++)
            for (int x = 0; x < PIC_W; x++) {
                int rv = rgbaToIndex(rgba[(size_t)y * PIC_W + x]);
                int gv = fb[(size_t)(ORIGIN_Y + y) * SCREEN_W + (ORIGIN_X + x)];
                if (rv != gv)
                    diffs++;
            }

        bool ok = diffs <= c.ceil;
        printf("%-10s %5d / %d mismatches (ceiling %d)  %s\n",
               c.name, diffs, PIC_W * PIC_H, c.ceil, ok ? "ok" : "REGRESSED");
        if (!ok)
            failures++;
    }

    if (failures) {
        printf("HDOS picture regression: %d case(s) failed\n", failures);
        return 1;
    }
    printf("HDOS picture regression: all %zu cases within ceiling\n",
           sizeof(kCases) / sizeof(kCases[0]));
    return 0;
}

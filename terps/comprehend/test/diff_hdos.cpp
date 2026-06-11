/* diff_hdos.cpp -- diagnostic: categorize DOS Talisman renderer diffs.
 * Throwaway tool: per-scene transition histogram + spatial diff PPM.
 */
#include "../hdos_talisman.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace Glk::Comprehend;

#define SCREEN_W 320
#define PIC_W 280
#define PIC_H 160
#define ORIGIN_X 20
#define ORIGIN_Y 0

struct Case { const char *name; const char *img; const char *fb; };
static const Case kCases[] = {
    { "title",     "test/hdos/title.img",     "test/hdos/title.fb" },
    { "throne",    "test/hdos/throne.img",    "test/hdos/throne.fb" },
    { "cell",      "test/hdos/cell.img",      "test/hdos/cell.fb" },
    { "courtyard", "test/hdos/courtyard.img", "test/hdos/courtyard.fb" },
};

static std::vector<uint8_t> readFile(const std::string &p) {
    std::vector<uint8_t> v; FILE *f = fopen(p.c_str(), "rb");
    if (!f) return v; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0) { v.resize((size_t)n); if (fread(v.data(), 1, n, f) != (size_t)n) v.clear(); }
    fclose(f); return v;
}
static int rgbaToIndex(uint32_t p) {
    // Accept both CGA palette-1 intensities so the diagnostic survives a
    // kHdosColor palette change (the game uses low intensity).
    switch (p >> 8) {
    case 0x000000: return 0;
    case 0x55ffff: case 0x00aaaa: return 1;
    case 0xff55ff: case 0xaa00aa: return 2;
    case 0xffffff: case 0xaaaaaa: return 3;
    default: return 0; }
}

int main() {
    auto tables = readFile("test/hdos/novel_tables.bin");
    if (tables.empty() || !hdosInstallDrawingTables(tables.data(), tables.size())) {
        fprintf(stderr, "cannot load tables\n"); return 1; }

    for (const Case &c : kCases) {
        auto img = readFile(c.img), fb = readFile(c.fb);
        if (img.empty() || fb.size() != (size_t)SCREEN_W * 200) { fprintf(stderr, "skip %s\n", c.name); continue; }
        hdosResetScreen(true);
        hdosDrawImage(img.data(), img.size());
        std::vector<uint32_t> rgba((size_t)PIC_W * PIC_H);
        hdosBlitToSurface(rgba.data(), PIC_W, PIC_H);

        int trans[4][4] = {{0}};
        int diffs = 0;
        // spatial PPM: green=match, red=diff
        std::string ppmpath = std::string("/tmp/diff_") + c.name + ".ppm";
        FILE *pp = fopen(ppmpath.c_str(), "wb");
        fprintf(pp, "P6\n%d %d\n255\n", PIC_W, PIC_H);
        // row histogram of diffs
        std::vector<int> rowdiff(PIC_H, 0), coldiff(PIC_W, 0);
        for (int y = 0; y < PIC_H; y++)
            for (int x = 0; x < PIC_W; x++) {
                int rv = rgbaToIndex(rgba[(size_t)y * PIC_W + x]);
                int gv = fb[(size_t)(ORIGIN_Y + y) * SCREEN_W + (ORIGIN_X + x)];
                unsigned char px[3];
                if (rv != gv) {
                    diffs++; trans[rv][gv]++; rowdiff[y]++; coldiff[x]++;
                    px[0] = 255; px[1] = 0; px[2] = 0;
                } else {
                    // shade by value so structure is visible
                    unsigned char g = (unsigned char)(40 + gv * 50);
                    px[0] = 0; px[1] = g; px[2] = 0;
                }
                fwrite(px, 1, 3, pp);
            }
        fclose(pp);

        printf("=== %s: %d diffs ===\n", c.name, diffs);
        printf("  transition (rendered->golden):\n");
        for (int r = 0; r < 4; r++)
            for (int g = 0; g < 4; g++)
                if (trans[r][g]) printf("    %d->%d : %d\n", r, g, trans[r][g]);
        // top diff rows
        printf("  diffs by row band:\n");
        for (int y0 = 0; y0 < PIC_H; y0 += 16) {
            int s = 0; for (int y = y0; y < y0 + 16 && y < PIC_H; y++) s += rowdiff[y];
            printf("    rows %3d-%3d: %d\n", y0, y0 + 15, s);
        }
        printf("  wrote %s\n", ppmpath.c_str());

        // raw index dump (rendered + golden) for external analysis
        std::string rp = std::string("/tmp/rend_") + c.name + ".raw";
        std::string gp = std::string("/tmp/gold_") + c.name + ".raw";
        FILE *rf = fopen(rp.c_str(), "wb"), *gf = fopen(gp.c_str(), "wb");
        for (int y = 0; y < PIC_H; y++)
            for (int x = 0; x < PIC_W; x++) {
                unsigned char rv = (unsigned char)rgbaToIndex(rgba[(size_t)y * PIC_W + x]);
                unsigned char gv = (unsigned char)fb[(size_t)(ORIGIN_Y + y) * SCREEN_W + (ORIGIN_X + x)];
                fwrite(&rv, 1, 1, rf); fwrite(&gv, 1, 1, gf);
            }
        fclose(rf); fclose(gf);
    }
    return 0;
}

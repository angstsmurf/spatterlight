/* gmcgav1test.cpp -- offline renderer for the v1 Comprehend DOS (CGA) games.
 *
 * Loads the v1 drawing tables from PC_GRAPH.OVR (fill pattern + subindex) and
 * NOVEL.EXE (brushes) via gmcgaInstallV1DrawingTables(), then renders one
 * picture out of a v1 .MS1 image file and writes a 280x160 PPM (and optionally
 * compares the 280x160 picture window against a ground-truth CGA framebuffer).
 *
 * Build:  c++ -std=c++14 -O0 -g -Wall -I.. -o gmcgav1test gmcgav1test.cpp ../graphics_magician_cga.cpp
 * Run:    ./gmcgav1test PC_GRAPH.OVR NOVEL.EXE RA.MS1 <picIndex> out.ppm [golden.fb]
 *
 * .MS1 offset table: version word 0x1000 -> 4-byte header, then 16 little-endian
 * offsets each biased by +4 (mirrors Pics::ImageFile + renderGmcga).
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

// CGA mode-4 de-interleave: 16 KB VRAM -> 320x200 palette indices.
static std::vector<uint8_t> vramToFb(const std::vector<uint8_t> &vram) {
    std::vector<uint8_t> fb(320 * 200, 0);
    if (vram.size() < 0x4000) return fb;
    for (int y = 0; y < 200; y++) {
        int base = (y & 1) * 0x2000 + (y >> 1) * 80;
        int o = y * 320;
        for (int xb = 0; xb < 80; xb++) {
            uint8_t b = vram[base + xb];
            fb[o]   = (b >> 6) & 3;
            fb[o+1] = (b >> 4) & 3;
            fb[o+2] = (b >> 2) & 3;
            fb[o+3] = b & 3;
            o += 4;
        }
    }
    return fb;
}

static uint8_t rgbaToIndex(uint32_t c) {
    switch (c) {
    case 0x000000ffu: return 0;
    case 0x00aaaaffu: return 1;
    case 0xaa00aaffu: return 2;
    case 0xaaaaaaffu: return 3;
    }
    return 0;
}

// Draw picture `pic` out of an already-loaded .MS1 onto the current canvas
// (no screen reset -- so a second call composites, as the engine does when it
// stamps an object over a room).  Returns false on a bad index.
// A title file (CCTITLE.MS1 / TRTITLE.MS1) holds a single image with no offset
// table; mirror Pics::ImageFile and start it at 4 when the file leads with a
// 0x00 header byte (no real stream opens with op0).  Pass pic index -1.
static bool drawMs1Pic(const std::vector<uint8_t> &ms1, int pic) {
    if (ms1.size() < 4) return false;
    if (pic < 0) {
        size_t at = (ms1[0] == 0) ? 4 : 0;
        gmcgaDrawImage(ms1.data() + at, ms1.size() - at);
        return true;
    }
    uint16_t version = (uint16_t)(ms1[0] | (ms1[1] << 8));
    size_t tableAt = (version == 0x1000) ? 4 : 0;
    uint16_t off[16];
    for (int i = 0; i < 16; i++) {
        size_t p = tableAt + i * 2;
        off[i] = (uint16_t)(ms1[p] | (ms1[p + 1] << 8));
        if (version == 0x1000) off[i] += 4;
    }
    if (pic < 0 || pic >= 16 || off[pic] >= ms1.size()) {
        fprintf(stderr, "bad pic index %d (offset %u, size %zu)\n", pic, off[pic], ms1.size());
        return false;
    }
    gmcgaDrawImage(ms1.data() + off[pic], ms1.size() - off[pic]);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s PC_GRAPH.OVR NOVEL.EXE file.MS1 picIndex out.ppm [golden.fb]\n", argv[0]);
        return 2;
    }
    std::vector<uint8_t> ovr = readFile(argv[1]);
    std::vector<uint8_t> exe = readFile(argv[2]);
    std::vector<uint8_t> ms1 = readFile(argv[3]);
    int pic = atoi(argv[4]);
    if (ovr.empty() || exe.empty() || ms1.empty()) return 1;

    if (!gmcgaInstallV1DrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size())) {
        fprintf(stderr, "gmcgaInstallV1DrawingTables failed\n");
        return 1;
    }
    // Optional: load the in-picture font for op3/op5 text (maps) from CHARSET.GDA
    // passed via the GMCGA_CHARSET env var.
    if (const char *cs = getenv("GMCGA_CHARSET")) {
        std::vector<uint8_t> charset = readFile(cs);
        if (!gmcgaSetV1Font(charset.data(), charset.size()))
            fprintf(stderr, "gmcgaSetV1Font(%s) failed\n", cs);
        else
            fprintf(stderr, "loaded font from %s\n", cs);
    }

    gmcgaResetScreen(true); // white cold page, as on a room change

    // Optional base layer: render a room picture first, then composite the
    // target picture on top of it (no reset between) -- this is how object
    // pictures are drawn over their room in the engine, and their flood fills
    // are bounded by the room's geometry.  Set GMCGA_BASE_MS1 + GMCGA_BASE_PIC.
    if (const char *bms1 = getenv("GMCGA_BASE_MS1")) {
        std::vector<uint8_t> base = readFile(bms1);
        int bpic = getenv("GMCGA_BASE_PIC") ? atoi(getenv("GMCGA_BASE_PIC")) : 0;
        if (base.empty() || !drawMs1Pic(base, bpic)) {
            fprintf(stderr, "base layer %s pic %d failed\n", bms1, bpic);
            return 1;
        }
        fprintf(stderr, "base layer: %s pic %d\n", bms1, bpic);
    }

    if (!drawMs1Pic(ms1, pic)) return 1;

    std::vector<uint32_t> rgba(PIC_W * PIC_H);
    gmcgaBlitToSurface(rgba.data(), PIC_W, PIC_H);

    // Write PPM.
    FILE *o = fopen(argv[5], "wb");
    if (o) {
        fprintf(o, "P6\n%d %d\n255\n", PIC_W, PIC_H);
        for (int i = 0; i < PIC_W * PIC_H; i++) {
            uint32_t c = rgba[i];
            uint8_t rgb[3] = { (uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8) };
            fwrite(rgb, 1, 3, o);
        }
        fclose(o);
        printf("wrote %s (%dx%d)\n", argv[5], PIC_W, PIC_H);
    }

    // Optional pixel-exact compare against a 16 KB CGA VRAM golden, in the
    // picture window at screen origin (20, 0).
    if (argc >= 7) {
        std::vector<uint8_t> golden = vramToFb(readFile(argv[6]));
        if (golden.size() == 320 * 200) {
            int mism = 0, first = -1;
            for (int y = 0; y < PIC_H; y++)
                for (int x = 0; x < PIC_W; x++) {
                    uint8_t got = rgbaToIndex(rgba[y * PIC_W + x]);
                    uint8_t exp = golden[y * 320 + (x + 20)];
                    if (got != exp) { if (first < 0) first = y * PIC_W + x; mism++; }
                }
            printf("compare: %d / %d mismatches%s\n", mism, PIC_W * PIC_H,
                   mism == 0 ? "  PIXEL-EXACT" : "");
            if (mism) printf("  first mismatch at x=%d y=%d\n", first % PIC_W, first / PIC_W);
            if (mism && getenv("GMCGA_DIFF")) {
                const char *nm = "kcmw";
                for (int y = 0; y < PIC_H; y++)
                    for (int x = 0; x < PIC_W; x++) {
                        uint8_t got = rgbaToIndex(rgba[y * PIC_W + x]);
                        uint8_t exp = golden[y * 320 + (x + 20)];
                        if (got != exp)
                            printf("    x=%d y=%d gx=%d byte=%d ph=%d render=%c golden=%c\n",
                                   x, y, x + 20, (x + 20) >> 2, (x + 20) & 3, nm[got], nm[exp]);
                    }
            }
        }
    }
    return 0;
}

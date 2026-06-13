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

    // Parse the .MS1 offset table.
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
        return 1;
    }

    gmcgaResetScreen(true); // white cold page, as on a room change
    gmcgaDrawImage(ms1.data() + off[pic], ms1.size() - off[pic]);

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
        }
    }
    return 0;
}

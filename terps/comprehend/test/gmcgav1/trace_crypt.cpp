/* trace_crypt.cpp -- single-picture trace driver for the crypt RA3 flood-fill
 * residual (TODO_v1_cga_graphics.md residual #1).  Renders ONLY crypt.img so the
 * renderer's per-fill counter `n` is local to this picture; GMCGA_TRACE_FILL=<n>
 * then logs the Nth op14's pushes + per-word paints (WORD seq=... lines).
 *
 * Build: c++ -std=c++14 -O0 -g -Wall -I../.. -o trace_crypt trace_crypt.cpp ../../graphics_magician_cga.cpp
 * Run:   GMCGA_TRACE_FILL=N ./trace_crypt   (cwd = terps/comprehend)
 */
#include "../../graphics_magician_cga.h"
#include <cstdio>
#include <cstdint>
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
            fb[o]=(b>>6)&3; fb[o+1]=(b>>4)&3; fb[o+2]=(b>>2)&3; fb[o+3]=b&3; o+=4;
        }
    }
    return fb;
}
static uint8_t rgbaToIndex(uint32_t c) {
    switch (c) {
    case 0x000000ffu: return 0; case 0x00aaaaffu: return 1;
    case 0xaa00aaffu: return 2; case 0xaaaaaaffu: return 3;
    } return 0;
}

int main() {
    std::vector<uint8_t> ovr = readFile("test/gmcgav1/fixtures/novel_ovr_tables.bin");
    std::vector<uint8_t> exe = readFile("test/gmcgav1/fixtures/novel_brushes.bin");
    std::vector<uint8_t> img = readFile("test/gmcgav1/fixtures/crypt.img");
    std::vector<uint8_t> golden = vramToFb(readFile("test/gmcgav1/fixtures/crypt.fb"));
    if (ovr.empty() || exe.empty() || img.empty()) return 1;
    if (!gmcgaInstallV1DrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size())) return 1;
    gmcgaResetScreen(true);
    gmcgaDrawImage(img.data(), img.size());
    std::vector<uint32_t> rgba(PIC_W * PIC_H);
    gmcgaBlitToSurface(rgba.data(), PIC_W, PIC_H);

    int mism = 0;
    for (int y = 0; y < PIC_H; y++)
        for (int x = 0; x < PIC_W; x++) {
            uint8_t got = rgbaToIndex(rgba[y * PIC_W + x]);
            uint8_t exp = golden[y * 320 + (x + 20)];
            if (got != exp) {
                mism++;
                const char *nm = "kcmw";
                printf("DIFF x=%d y=%d gx=%d byte=%d ph=%d render=%c golden=%c\n",
                       x, y, x + 20, (x + 20) >> 2, (x + 20) & 3, nm[got], nm[exp]);
            }
        }
    printf("crypt: %d mismatches\n", mism);
    return 0;
}

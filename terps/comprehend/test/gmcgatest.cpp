/* gmcgatest.cpp -- offline harness for the DOS Talisman CGA renderer.
 *
 * Loads the drawing tables out of a NOVEL.EXE / NOVEL1.EXE image, renders one
 * Graphics Magician vector image, and writes the result as a binary PPM (P6)
 * so it can be eyeballed against a DOSBox capture.
 *
 * Usage: gmcgatest <NOVEL.EXE> <image-file> <data-offset> <out.ppm> [white|black]
 *   image-file  = T0 (title), RA.. (room), OA.. (item) ...
 *   data-offset = byte offset of the vector stream in image-file (T0 = 4)
 */

#include "../graphics_magician_cga.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

using namespace Glk::Comprehend;

static std::vector<uint8_t> readFile(const char *path) {
    std::vector<uint8_t> v;
    FILE *f = fopen(path, "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0) { v.resize((size_t)n); if (fread(v.data(), 1, (size_t)n, f) != (size_t)n) v.clear(); }
    fclose(f);
    return v;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <NOVEL.EXE> <image> <offset> <out.ppm> [white|black]\n", argv[0]);
        return 2;
    }
    std::vector<uint8_t> exe = readFile(argv[1]);
    if (exe.empty()) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    if (!gmcgaInstallDrawingTables(exe.data(), exe.size())) {
        fprintf(stderr, "drawing tables not found in %s\n", argv[1]); return 1;
    }

    std::vector<uint8_t> img = readFile(argv[2]);
    if (img.empty()) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
    size_t off = (size_t)strtol(argv[3], nullptr, 0);
    if (off >= img.size()) { fprintf(stderr, "offset past end\n"); return 1; }

    bool white = !(argc >= 6 && strcmp(argv[5], "black") == 0);
    gmcgaResetScreen(white);
    gmcgaDrawImage(img.data() + off, img.size() - off);

    const int W = 280, H = 160;
    std::vector<uint32_t> rgba((size_t)W * H);
    gmcgaBlitToSurface(rgba.data(), W, H);

    FILE *o = fopen(argv[4], "wb");
    if (!o) { fprintf(stderr, "cannot write %s\n", argv[4]); return 1; }
    fprintf(o, "P6\n%d %d\n255\n", W, H);
    for (size_t i = 0; i < rgba.size(); i++) {
        uint32_t p = rgba[i]; // 0xRRGGBBAA
        uint8_t rgb[3] = { (uint8_t)(p >> 24), (uint8_t)(p >> 16), (uint8_t)(p >> 8) };
        fwrite(rgb, 1, 3, o);
    }
    fclose(o);
    return 0;
}

// pcjrtest.cpp -- offline harness for graphics_magician_pcjr.cpp.
//
// Loads JR_GRAPH.OVR + NOVEL(1).EXE + CHARSET.GDA, installs the PCjr drawing
// tables, renders one image from a .MS1 picture file, and writes a PPM.
//
//   c++ -std=c++17 -I.. pcjrtest.cpp ../graphics_magician_pcjr.cpp -o pcjrtest
//   ./pcjrtest <gamedir> <RA.MS1> <imageIndex> <out.ppm>
#include "../../graphics_magician_pcjr.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace Glk::Comprehend;

static std::vector<uint8_t> readFile(const std::string &p) {
    std::vector<uint8_t> v;
    FILE *f = fopen(p.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    v.resize(n);
    if (fread(v.data(), 1, n, f) != (size_t)n) v.clear();
    fclose(f);
    return v;
}

// Locate a file case-insensitively in dir.
static std::string findFile(const std::string &dir, const std::string &name) {
    for (const std::string &n : { name, name + "" }) { (void)n; }
    std::string lower = dir + "/" + name;
    if (FILE *f = fopen(lower.c_str(), "rb")) { fclose(f); return lower; }
    // try lowercase
    std::string ln = name;
    for (char &c : ln) c = (char)tolower(c);
    std::string p2 = dir + "/" + ln;
    if (FILE *f = fopen(p2.c_str(), "rb")) { fclose(f); return p2; }
    return lower;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <gamedir> <pic.MS1> <imageIndex> <out.ppm>\n", argv[0]);
        return 2;
    }
    std::string dir = argv[1];
    std::string picName = argv[2];
    int imgIndex = atoi(argv[3]);
    std::string outPath = argv[4];

    auto ovr = readFile(findFile(dir, "JR_GRAPH.OVR"));
    auto exe = readFile(findFile(dir, "NOVEL1.EXE"));
    if (exe.empty()) exe = readFile(findFile(dir, "NOVEL.EXE"));
    auto charset = readFile(findFile(dir, "CHARSET.GDA"));
    if (ovr.empty() || exe.empty()) {
        fprintf(stderr, "missing JR_GRAPH.OVR or NOVEL.EXE in %s\n", dir.c_str());
        return 1;
    }
    if (!gmpcjrInstallDrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size())) {
        fprintf(stderr, "gmpcjrInstallDrawingTables failed\n");
        return 1;
    }
    if (!charset.empty())
        gmpcjrSetFont(charset.data(), charset.size());

    auto pic = readFile(findFile(dir, picName));
    if (pic.empty()) { fprintf(stderr, "cannot read %s\n", picName.c_str()); return 1; }

    uint32_t start;
    if (imgIndex < 0) {
        // Title file (single image): 4-byte header iff the first byte is 0,
        // matching pics.cpp's title sniff for the v1 .MS1 titles.
        start = (pic[0] == 0) ? 4 : 0;
    } else {
        // Parse .MS1 image offsets (version word; if 0x1000, +4).
        uint16_t version = (uint16_t)(pic[0] | (pic[1] << 8));
        size_t tableStart = (version == 0x1000) ? 4 : 0;
        std::vector<uint32_t> offs(16);
        for (int i = 0; i < 16; i++) {
            size_t o = tableStart + i * 2;
            offs[i] = (uint16_t)(pic[o] | (pic[o + 1] << 8));
            if (version == 0x1000) offs[i] += 4;
        }
        if (imgIndex >= 16) { fprintf(stderr, "bad index\n"); return 1; }
        start = offs[imgIndex];
    }
    if (start >= pic.size()) { fprintf(stderr, "image offset out of range (%u)\n", start); return 1; }

    gmpcjrResetScreen(true); // white room background
    gmpcjrDrawImage(pic.data() + start, pic.size() - start);

    const int W = 280, H = 160;
    std::vector<uint32_t> rgba(W * H);
    gmpcjrBlitToSurface(rgba.data(), W, H);

    FILE *f = fopen(outPath.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", outPath.c_str()); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint32_t c = rgba[i];
        uint8_t rgb[3] = { (uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    fprintf(stderr, "wrote %s (%dx%d), image %d @0x%x of %s\n",
            outPath.c_str(), W, H, imgIndex, start, picName.c_str());
    return 0;
}

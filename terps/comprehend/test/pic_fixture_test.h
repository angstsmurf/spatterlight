/* pic_fixture_test.h -- helpers shared by the picture regression drivers
 * (test_talisman_pics, test_gmcga_pics, test_gmcgav1_pics, test_dhgr_pics,
 * test_dhgr_determinism): reading a fixture file, the Apple II hi-res row
 * layout, and comparing a rendered CGA picture against a DOSBox golden.
 *
 * Header-only and renderer-independent, so each driver still links only the
 * renderer it tests.
 */

#ifndef GLK_COMPREHEND_TEST_PIC_FIXTURE_TEST_H
#define GLK_COMPREHEND_TEST_PIC_FIXTURE_TEST_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Whole file as bytes; empty if it is missing, empty or unreadable.
static inline std::vector<uint8_t> readFile(const std::string &path) {
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

// --- Apple II hi-res pages ---------------------------------------------------

// Rows 0..159 are the picture; 160..191 hold the game's text, which the Glk
// port renders separately, so the comparisons stop at kHiresPictureRows.
enum { kHiresRows = 192, kHiresPictureRows = 160, kHiresPageSize = 0x2000 };

// Offset of row y's first byte within a hi-res page.
static inline int hiresRowBase(int y) {
	return (((y / 8) & 7) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10);
}

// The row a page offset belongs to, or -1 for the unused "screen hole" bytes.
static inline int hiresRowOfOffset(int off) {
	for (int row = 0; row < kHiresRows; row++) {
		int base = hiresRowBase(row);
		if (off >= base && off < base + 40)
			return row;
	}
	return -1;
}

// Differing pixel bits between two double hi-res screens (main + aux page each)
// over rows [lo, hi).
static inline int dhgrDiffBits(const uint8_t *main1, const uint8_t *aux1,
							   const uint8_t *main2, const uint8_t *aux2, int lo, int hi) {
	int d = 0;
	for (int y = lo; y < hi; y++) {
		int base = hiresRowBase(y);
		for (int col = 0; col < 40; col++) {
			d += __builtin_popcount((main1[base + col] ^ main2[base + col]) & 0x7f);
			d += __builtin_popcount((aux1[base + col] ^ aux2[base + col]) & 0x7f);
		}
	}
	return d;
}

// --- DOS CGA pictures --------------------------------------------------------

// The DOS screen is 320x200; the picture window is 280x160 at (20, 0).
enum { kCgaScreenW = 320, kCgaScreenH = 200, kCgaPicW = 280, kCgaPicH = 160, kCgaOriginX = 20 };

// A renderer RGBA pixel (0xRRGGBBAA, kGmcgaColor) as its CGA palette index.
static inline int cgaRgbaToIndex(uint32_t p) {
	switch (p >> 8) {  // drop alpha
	case 0x000000: return 0; // black
	case 0x00aaaa: return 1; // cyan    (low intensity)
	case 0xaa00aa: return 2; // magenta (low intensity)
	case 0xaaaaaa: return 3; // grey    (low intensity "white")
	default:       return 0;
	}
}

// Unpack a 16 KB CGA VRAM dump (two interlaced 2-bits-per-pixel banks) into a
// 320x200 framebuffer of palette indices.
static inline std::vector<uint8_t> cgaVramToFb(const std::vector<uint8_t> &vram) {
	std::vector<uint8_t> fb(kCgaScreenW * kCgaScreenH, 0);
	if (vram.size() < 0x4000) return fb;
	for (int y = 0; y < kCgaScreenH; y++) {
		int base = (y & 1) * 0x2000 + (y >> 1) * 80, o = y * kCgaScreenW;
		for (int xb = 0; xb < 80; xb++) {
			uint8_t b = vram[base + xb];
			fb[o] = (b >> 6) & 3; fb[o+1] = (b >> 4) & 3;
			fb[o+2] = (b >> 2) & 3; fb[o+3] = b & 3;
			o += 4;
		}
	}
	return fb;
}

// Mismatching pixels between a rendered 280x160 RGBA picture and the picture
// window of a 320x200 framebuffer of palette indices.
static inline int cgaCountMismatches(const std::vector<uint32_t> &rgba, const std::vector<uint8_t> &fb) {
	int diffs = 0;
	for (int y = 0; y < kCgaPicH; y++)
		for (int x = 0; x < kCgaPicW; x++)
			if (cgaRgbaToIndex(rgba[(size_t)y * kCgaPicW + x]) != fb[(size_t)y * kCgaScreenW + (kCgaOriginX + x)])
				diffs++;
	return diffs;
}

#endif

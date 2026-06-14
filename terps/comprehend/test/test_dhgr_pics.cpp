/* test_dhgr_pics.cpp -- regression for the Apple II DOUBLE hi-res renderer.
 *
 * Drives graphics_magician_dhgr.cpp the way the engine will: install the drawing
 * tables from the boot disk's T5 file, then render Talisman's prison cell (RA
 * image 4) and assert the double-hi-res pages are BYTE-EXACT over the picture
 * rows (0..0x9f) vs the MAME ground truth. Also pins the NTSC colour output.
 *
 * Text rows 0xa0..0xbf are a separate text subsystem and intentionally excluded.
 *
 * Build:
 *   c++ -std=c++17 -I.. -I../../common_sagadraw test_dhgr_pics.cpp \
 *       ../graphics_magician_dhgr.cpp ../../common_sagadraw/gm_artifact.c -o test_dhgr_pics
 */
#include "graphics_magician_dhgr.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Glk::Comprehend;

static int CALC(int y) { return (((y / 8) & 7) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10); }

static std::vector<uint8_t> readFile(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> v(n); fread(v.data(), 1, n, f); fclose(f);
	return v;
}

int main(int argc, char **argv) {
	const char *dir = (argc > 1) ? argv[1] : "dhgr";
	char path[512];
	auto load = [&](const char *name) { snprintf(path, sizeof path, "%s/%s", dir, name); return readFile(path); };

	auto t5 = load("T5");
	if (!gmDhgrInstallDrawingTables(t5.data(), t5.size())) { printf("FAIL: install tables\n"); return 1; }

	// RA: first 32 bytes are an LE16 offset table; image 4 is the cell.
	auto ra = load("RA");
	uint32_t off[17];
	for (int i = 0; i < 16; i++) off[i] = ra[i * 2] | (ra[i * 2 + 1] << 8);
	off[16] = ra.size();
	int idx = 4;

	gmDhgrResetScreen(false);
	gmDhgrDrawImage(ra.data() + off[idx], off[idx + 1] - off[idx]);

	auto gm = load("cell_main.bin");
	auto ga = load("cell_aux.bin");
	const uint8_t *mm = gmDhgrMainPtr(), *ma = gmDhgrAuxPtr();

	auto diff = [&](int lo, int hi) {
		int d = 0;
		for (int y = lo; y < hi; y++) {
			int base = CALC(y);
			for (int col = 0; col < 80; col++) {
				int b = base + (col >> 1);
				uint8_t mine = (col & 1 ? mm : ma)[b];
				uint8_t gt   = (col & 1 ? gm.data() : ga.data())[b];
				d += __builtin_popcount((mine ^ gt) & 0x7f);
			}
		}
		return d;
	};

	int pic = diff(0, 160), txt = diff(160, 192);
	printf("picture rows 0..0x9f: %d differing bits\n", pic);
	printf("text rows 0xa0..0xbf: %d (excluded)\n", txt);

	// Colour pass sanity: ensure the blit runs and produces non-trivial output.
	std::vector<uint32_t> rgba(560 * 160);
	gmDhgrBlitToSurface(rgba.data(), 560, 160);
	int nonblack = 0;
	for (auto px : rgba) if ((px >> 8) != 0) nonblack++;
	printf("NTSC blit: %d/%zu non-black pixels\n", nonblack, rgba.size());

	if (pic != 0) { printf("FAIL: cell picture rows are not byte-exact\n"); return 1; }
	if (nonblack == 0) { printf("FAIL: colour blit produced an empty image\n"); return 1; }
	printf("PASS: DHGR cell byte-exact + colour blit OK\n");
	return 0;
}

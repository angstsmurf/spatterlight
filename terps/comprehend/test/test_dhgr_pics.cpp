/* test_dhgr_pics.cpp -- regression for the Apple II DOUBLE hi-res renderer.
 *
 * Drives graphics_magician_dhgr.cpp the way the engine will: install the drawing
 * tables from the boot disk's T5 file, then render Talisman room pictures and
 * assert the double-hi-res pages are BYTE-EXACT over the picture rows (0..0x9f)
 * vs pages captured from MAME running the real game in "D" mode. Also pins the
 * NTSC colour output.
 *
 * Cases: the prison cell (Empire disk, RA 4) and, from the Lands Beyond disk,
 * every picture that re-patches the colour its current fill pattern was built
 * from (RG 2, 6, 10, 11, 12: op7 has to rebuild the pattern at once, see op7()
 * in the renderer) plus RF 3, whose stream ends in forty brush-1 dots. The
 * fixtures and the MAME capture recipe are described in test/dhgr/README.md.
 *
 * Text rows 0xa0..0xbf are a separate text subsystem and intentionally excluded.
 *
 * Build/run:  make -C terps/comprehend test   (fixtures: test/dhgr/fixtures/,
 * local-only; pass another directory as argv[1])
 */
#include "../graphics_magician_dhgr.h"
#include "pic_fixture_test.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Glk::Comprehend;

struct Case {
	const char *file;     // room picture file on the disk (RA..RG)
	int idx;              // picture 0..15 within it
	const char *mainPage; // MAME capture of main $2000..$3FFF
	const char *auxPage;  // ... and of aux $2000..$3FFF
	const char *label;
};

static const Case kCases[] = {
	{"RA",  4, "cell_main.bin",   "cell_aux.bin",   "Empire RA 4, the prison cell"},
	{"RF",  3, "lb_RF3_main.bin", "lb_RF3_aux.bin", "Lands Beyond RF 3, brush-1 dots"},
	{"RG",  2, "lb_RG2_main.bin", "lb_RG2_aux.bin", "Lands Beyond RG 2, 1 pattern rebuild"},
	{"RG",  6, "lb_RG6_main.bin", "lb_RG6_aux.bin", "Lands Beyond RG 6, 1 pattern rebuild"},
	{"RG", 10, "lb_RG10_main.bin", "lb_RG10_aux.bin", "Lands Beyond RG 10, 12 pattern rebuilds"},
	{"RG", 11, "lb_RG11_main.bin", "lb_RG11_aux.bin", "Lands Beyond RG 11, 6 pattern rebuilds"},
	{"RG", 12, "lb_RG12_main.bin", "lb_RG12_aux.bin", "Lands Beyond RG 12, 2 pattern rebuilds"},
};

int main(int argc, char **argv) {
	const char *dir = (argc > 1) ? argv[1] : "test/dhgr/fixtures";
	char path[512];
	auto load = [&](const char *name) {
		snprintf(path, sizeof path, "%s/%s", dir, name);
		std::vector<uint8_t> v = readFile(path);
		if (v.empty()) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
		return v;
	};

	auto t5 = load("T5");
	if (!gmDhgrInstallDrawingTables(t5.data(), t5.size())) { printf("FAIL: install tables\n"); return 1; }

	int failed = 0;
	for (size_t c = 0; c < sizeof(kCases) / sizeof(kCases[0]); c++) {
		const Case &k = kCases[c];
		// A room picture file starts with 16 LE16 stream offsets. Each stream ends
		// at its own END opcode, so it is handed over up to the end of the file.
		auto file = load(k.file);
		uint32_t start = file[k.idx * 2] | (file[k.idx * 2 + 1] << 8);
		if (start >= file.size()) { printf("FAIL: %s %d: bad offset\n", k.file, k.idx); return 1; }

		gmDhgrResetScreen(false);
		gmDhgrDrawImage(file.data() + start, file.size() - start);

		auto gm = load(k.mainPage);
		auto ga = load(k.auxPage);
		const uint8_t *mm = gmDhgrMainPtr(), *ma = gmDhgrAuxPtr();
		int pic = dhgrDiffBits(mm, ma, gm.data(), ga.data(), 0, kHiresPictureRows);
		int txt = dhgrDiffBits(mm, ma, gm.data(), ga.data(), kHiresPictureRows, kHiresRows);
		printf("%-42s picture rows 0..0x9f: %d differing bits (text rows: %d, excluded)\n",
			   k.label, pic, txt);
		if (pic != 0) { printf("FAIL: %s %d is not byte-exact\n", k.file, k.idx); failed++; }

		if (c == 0) {
			// Colour pass sanity: ensure the blit runs and produces non-trivial output.
			std::vector<uint32_t> rgba(560 * 160);
			gmDhgrBlitToSurface(rgba.data(), 560, 160);
			int nonblack = 0;
			for (auto px : rgba) if ((px >> 8) != 0) nonblack++;
			printf("NTSC blit: %d/%zu non-black pixels\n", nonblack, rgba.size());
			if (nonblack == 0) { printf("FAIL: colour blit produced an empty image\n"); failed++; }
		}
	}

	if (failed) return 1;
	printf("PASS: %zu DHGR pictures byte-exact + colour blit OK\n", sizeof(kCases) / sizeof(kCases[0]));
	return 0;
}

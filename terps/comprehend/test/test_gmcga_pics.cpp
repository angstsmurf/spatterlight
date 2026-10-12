/* test_gmcga_pics.cpp -- regression test for the DOS Talisman CGA renderer.
 *
 * Each case pairs a picture's raw Graphics-Magician vector stream (extracted
 * from the DOS release of Talisman, NOVEL1.EXE / RA / T0) with a byte-exact
 * ground-truth framebuffer captured from DOSBox-X.  The test renders the stream
 * through graphics_magician_cga.cpp and compares the 280x160 picture window, at screen
 * origin (20, 0), against the golden in CGA palette-index space (0=black,
 * 1=cyan, 2=magenta, 3=grey) -- so the high/low intensity palette choice does
 * not matter (cgaRgbaToIndex below maps whatever RGB kGmcgaColor uses back to the
 * index, and the .fb goldens already store indices).
 *
 * Self-contained: needs neither DOSBox nor the copyrighted NOVEL1.EXE at test
 * time.  The drawing tables (fill / subindex / brush / font) are committed as a
 * small slice of NOVEL1.EXE in test/gmcga/fixtures/novel_tables.bin; each scene's vector
 * stream is test/gmcga/<scene>.img; each golden 320x200 framebuffer is
 * test/gmcga/<scene>.fb.  See test/gmcga/README.md for how the goldens were made.
 *
 * Build/run:  make -C terps/comprehend test
 *
 * The renderer is pixel-exact on every fixture, so every ceiling is 0: the
 * golden set is locked and any renderer change must keep this at 0 diffs.
 */

#include "../graphics_magician_cga.h"
#include "pic_fixture_test.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace Glk::Comprehend;

struct Case {
	const char *name;
	const char *img;   // vector stream file
	const char *fb;    // golden framebuffer file
	int         ceil;  // max tolerated mismatching pixels (current baseline)
};

static const Case kCases[] = {
	{ "title",     "test/gmcga/fixtures/title.img",     "test/gmcga/fixtures/title.fb",     0 },
	{ "throne",    "test/gmcga/fixtures/throne.img",    "test/gmcga/fixtures/throne.fb",    0 },
	{ "cell",      "test/gmcga/fixtures/cell.img",      "test/gmcga/fixtures/cell.fb",      0 },
	{ "courtyard", "test/gmcga/fixtures/courtyard.img", "test/gmcga/fixtures/courtyard.fb", 0 },
	// Title screens of the other v2 DOS releases (same drawing tables as
	// Talisman, byte-identical in their NOVEL.EXEs). The .img files are the
	// complete T0 -- a raw vector stream from byte 0, with no header.
	{ "oo_title",  "test/gmcga/fixtures/oo_title.img",  "test/gmcga/fixtures/oo_title.fb",  0 },
	{ "tr_title",  "test/gmcga/fixtures/tr_title.img",  "test/gmcga/fixtures/tr_title.fb",  0 },
};

// Room-fixture pass: every Talisman DOS room picture (RA..RG) captured from
// DOSBox by test/gmcga/dosbox_capture_pics.py and validated pixel-exact over the
// 280x160 window.  Streams (Graphics-Magician vector data sliced from the game
// RA..RG files) and goldens (the window packed 2 bits/pixel, MSB-first) are
// committed as two blobs plus a manifest, so this needs no game files at test
// time.  See test/gmcga/gen_room_fixtures.py.
static int runRoomFixtures() {
	FILE *mf = fopen("test/gmcga/rooms.tsv", "r");
	if (!mf)
		return 0;                       // optional: skip if not generated
	std::vector<uint8_t> streams = readFile("test/gmcga/fixtures/rooms_streams.bin");
	std::vector<uint8_t> goldens = readFile("test/gmcga/fixtures/rooms_goldens.bin");
	if (streams.empty() || goldens.empty()) {
		fprintf(stderr, "FAIL: rooms.tsv present but blobs missing/empty\n");
		fclose(mf);
		return 1;
	}
	char line[256];
	if (!fgets(line, sizeof line, mf)) { fclose(mf); return 1; }   // header
	int fails = 0, n = 0, exact = 0;
	char name[32];
	long so, sl, go;
	int ceil;
	while (fgets(line, sizeof line, mf)) {
		if (sscanf(line, "%31s %ld %ld %ld %d", name, &so, &sl, &go, &ceil) != 5)
			continue;
		gmcgaResetScreen(true);          // white cold page
		gmcgaDrawImage(streams.data() + so, (size_t)sl);
		std::vector<uint32_t> rgba((size_t)kCgaPicW * kCgaPicH);
		gmcgaBlitToSurface(rgba.data(), kCgaPicW, kCgaPicH);
		int diffs = 0;
		for (int i = 0; i < kCgaPicW * kCgaPicH; i++) {
			int rv = cgaRgbaToIndex(rgba[i]);
			uint8_t pb = goldens[(size_t)go + (i >> 2)];   // 4 px/byte, MSB-first
			int gv = (pb >> ((3 - (i & 3)) * 2)) & 3;
			if (rv != gv)
				diffs++;
		}
		n++;
		if (diffs <= ceil) {
			if (diffs == 0) exact++;
		} else {
			fprintf(stderr, "ROOM %-8s %5d diffs (ceiling %d)  REGRESSED\n",
					name, diffs, ceil);
			fails++;
		}
	}
	fclose(mf);
	printf("GMCGA rooms: %d/%d pixel-exact, %d within ceiling, %d failed\n",
		   exact, n, n - fails, fails);
	return fails;
}

int main() {
	std::vector<uint8_t> tables = readFile("test/gmcga/fixtures/novel_tables.bin");
	if (tables.empty() || !gmcgaInstallDrawingTables(tables.data(), tables.size())) {
		fprintf(stderr, "FAIL: cannot load drawing tables from test/gmcga/fixtures/novel_tables.bin\n");
		return 1;
	}

	int failures = 0;
	for (const Case &c : kCases) {
		std::vector<uint8_t> img = readFile(c.img);
		std::vector<uint8_t> fb  = readFile(c.fb);
		if (img.empty()) { fprintf(stderr, "FAIL %s: cannot read %s\n", c.name, c.img); failures++; continue; }
		if (fb.size() != (size_t)kCgaScreenW * kCgaScreenH) {
			fprintf(stderr, "FAIL %s: %s wrong size %zu\n", c.name, c.fb, fb.size());
			failures++;
			continue;
		}

		gmcgaResetScreen(true);          // cold page: solid white
		gmcgaDrawImage(img.data(), img.size());

		std::vector<uint32_t> rgba((size_t)kCgaPicW * kCgaPicH);
		gmcgaBlitToSurface(rgba.data(), kCgaPicW, kCgaPicH);

		int diffs = cgaCountMismatches(rgba, fb);

		bool ok = diffs <= c.ceil;
		printf("%-10s %5d / %d mismatches (ceiling %d)  %s\n",
			   c.name, diffs, kCgaPicW * kCgaPicH, c.ceil, ok ? "ok" : "REGRESSED");
		if (!ok)
			failures++;
	}

	failures += runRoomFixtures();

	if (failures) {
		printf("GMCGA picture regression: %d case(s) failed\n", failures);
		return 1;
	}
	printf("GMCGA picture regression: all %zu scene cases + room fixtures within ceiling\n",
		   sizeof(kCases) / sizeof(kCases[0]));
	return 0;
}

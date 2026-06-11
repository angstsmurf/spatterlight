/* test_talisman_pics.cpp -- regression test for the Apple II Talisman renderer.
 *
 * Each case pairs a picture's raw vector-opcode stream (extracted from the game
 * disk) with a golden Apple II hi-res page captured from MAME running the real
 * game. The test renders the stream through graphics_magician.cpp and asserts the
 * resulting hi-res page matches MAME byte-for-byte over the picture rows.
 *
 * Self-contained: needs neither MAME nor the .woz disks at test time. To add a
 * case, capture a room's page 1 ($2000..$3FFF) from MAME and the matching image
 * bytes (see test/pixtest.cpp) into test/talisman/<name>.{img,page}.
 * Full procedure: test/talisman/README.md.
 *
 * Build/run:  make -C terps/comprehend test
 *
 * The comparison covers the picture area only (Apple rows 0..159); rows 160..191
 * hold the game's text overlay, which the Glk port renders separately.
 */

#include "../graphics_magician.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>

using namespace Glk::Comprehend;

#define PICTURE_ROWS 160   // rows 0..159 are the picture; 160..191 are text

static int rowOfOffset(int off) {
	// Invert the Apple hi-res address: off = base(row) + col, col 0..39.
	for (int row = 0; row < 192; row++) {
		int base = (((row / 8) & 7) << 7) + (((row / 8) & 0x18) * 5) + ((row & 7) << 10);
		if (off >= base && off < base + 40)
			return row;
	}
	return -1;
}

static std::vector<uint8_t> readFile(const std::string &path) {
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

struct Case {
	const char *name;
	const char *img;
	const char *page;
	bool whiteBg;
	bool legacy;     // older Graphics Magician dialect (Transylvania et al.)
	const char *t2;  // drawing-tables file; located by signature, layout differs
};

static const Case kCases[] = {
	// Talisman (Empire disk, bright rooms, newer dialect). courtyard exercises the
	// DRAW_BOX -> far-corner pen fix ($095b/$0990).
	{ "talisman courtyard",   "test/talisman/courtyard.img",   "test/talisman/courtyard.page",   true, false, "test/talisman/t2.bin" },
	{ "talisman executioner", "test/talisman/executioner.img", "test/talisman/executioner.page", true, false, "test/talisman/t2.bin" },
	{ "talisman cell",        "test/talisman/cell.img",        "test/talisman/cell.page",        true, false, "test/talisman/t2.bin" },
	// Transylvania (older dialect: op7/op15 end, full-screen fill). Its T2 places
	// the shared tables at a different offset, exercising the signature locator.
	// stump is the start room (lines + flood fill); cave adds brushes.
	{ "transylvania stump",   "test/transylvania/stump.img",   "test/transylvania/stump.page",   true, true,  "test/transylvania/t2.bin" },
	{ "transylvania cave",    "test/transylvania/cave.img",    "test/transylvania/cave.page",    true, true,  "test/transylvania/t2.bin" },
	// OO-Topos (newer dialect like Talisman: op15 sub-op 3 fills the background).
	// cell is the start room (standard-hires "S" mode).
	{ "ootopos cell",         "test/ootopos/cell.img",         "test/ootopos/cell.page",         true, false, "test/ootopos/t2.bin" },
	// OO-Topos title (spaceship + planet), a single vector image inside T0 past
	// the disk-protection stub. Unlike the rooms it starts from a BLACK page (it
	// fills its own background; a white start corrupts the "OO-TOPOS" lettering).
	{ "ootopos title",        "test/ootopos/title.img",        "test/ootopos/title.page",        false, false, "test/ootopos/t2.bin" },
	// Crimson Crown (older dialect, full-screen fill). lakeshore + woods.
	{ "crimsoncrown lakeshore","test/crimsoncrown/lakeshore.img","test/crimsoncrown/lakeshore.page",true, true,  "test/crimsoncrown/t2.bin" },
	{ "crimsoncrown woods",   "test/crimsoncrown/woods.img",   "test/crimsoncrown/woods.page",   true, true,  "test/crimsoncrown/t2.bin" },
};

int main(int argc, char **argv) {
	// Allow running from the comprehend dir or its parent; prepend an optional
	// path prefix (argv[1]) so `make test` can pass the source dir.
	std::string prefix = (argc > 1) ? std::string(argv[1]) + "/" : "";

	int failures = 0;
	for (const Case &c : kCases) {
		// The drawing tables come from the game's own T2 file; the signature
		// locator finds them wherever that release placed them.
		std::vector<uint8_t> t2 = readFile(prefix + c.t2);
		if (t2.empty() || !gmInstallDrawingTables(t2.data(), t2.size())) {
			fprintf(stderr, "FAIL %-22s : could not load %s (size=%zu)\n",
				c.name, c.t2, t2.size());
			failures++;
			continue;
		}

		std::vector<uint8_t> img  = readFile(prefix + c.img);
		std::vector<uint8_t> gold = readFile(prefix + c.page);
		if (img.empty() || gold.size() != 0x2000) {
			fprintf(stderr, "FAIL %-22s : missing/short fixture (img=%zu page=%zu)\n",
				c.name, img.size(), gold.size());
			failures++;
			continue;
		}

		gmSetLegacyFormat(c.legacy);
		gmResetScreen(c.whiteBg);
		gmDrawImage(img.data(), img.size());
		const uint8_t *page = gmPagePtr();

		int diffs = 0, firstOff = -1;
		for (int off = 0; off < 0x2000; off++) {
			int row = rowOfOffset(off);
			if (row < 0 || row >= PICTURE_ROWS) continue;
			if (page[off] != gold[off]) {
				if (firstOff < 0) firstOff = off;
				diffs++;
			}
		}

		if (diffs == 0) {
			printf("PASS %-22s : byte-exact over picture rows\n", c.name);
		} else {
			printf("FAIL %-22s : %d differing byte(s); first at off=%04x (row %d) "
			       "got=%02x want=%02x\n",
			       c.name, diffs, firstOff, rowOfOffset(firstOff),
			       page[firstOff], gold[firstOff]);
			failures++;
		}
	}

	printf("\n%d/%d cases passed\n", (int)(sizeof(kCases)/sizeof(kCases[0])) - failures,
	       (int)(sizeof(kCases)/sizeof(kCases[0])));
	return failures ? 1 : 0;
}

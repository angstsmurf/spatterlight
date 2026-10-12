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
#include "pic_fixture_test.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>

using namespace Glk::Comprehend;

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
	{ "talisman courtyard",   "test/talisman/fixtures/courtyard.img",   "test/talisman/fixtures/courtyard.page",   true, false, "test/talisman/fixtures/t2.bin" },
	{ "talisman executioner", "test/talisman/fixtures/executioner.img", "test/talisman/fixtures/executioner.page", true, false, "test/talisman/fixtures/t2.bin" },
	{ "talisman cell",        "test/talisman/fixtures/cell.img",        "test/talisman/fixtures/cell.page",        true, false, "test/talisman/fixtures/t2.bin" },
	// Transylvania (older dialect: op7/op15 end, full-screen fill). Its T2 places
	// the shared tables at a different offset, exercising the signature locator.
	// stump is the start room (lines + flood fill); cave adds brushes.
	{ "transylvania stump",   "test/transylvania/fixtures/stump.img",   "test/transylvania/fixtures/stump.page",   true, true,  "test/transylvania/fixtures/t2.bin" },
	{ "transylvania cave",    "test/transylvania/fixtures/cave.img",    "test/transylvania/fixtures/cave.page",    true, true,  "test/transylvania/fixtures/t2.bin" },
	// OO-Topos (newer dialect like Talisman: op15 sub-op 3 fills the background).
	// cell is the start room (standard-hires "S" mode).
	{ "ootopos cell",         "test/ootopos/fixtures/cell.img",         "test/ootopos/fixtures/cell.page",         true, false, "test/ootopos/fixtures/t2.bin" },
	// OO-Topos title (spaceship + planet), a single vector image inside T0 past
	// the disk-protection stub. Unlike the rooms it starts from a BLACK page (it
	// fills its own background; a white start corrupts the "OO-TOPOS" lettering).
	{ "ootopos title",        "test/ootopos/fixtures/title.img",        "test/ootopos/fixtures/title.page",        false, false, "test/ootopos/fixtures/t2.bin" },
	// Crimson Crown (older dialect, full-screen fill). lakeshore + woods.
	{ "crimsoncrown lakeshore","test/crimsoncrown/fixtures/lakeshore.img","test/crimsoncrown/fixtures/lakeshore.page",true, true,  "test/crimsoncrown/fixtures/t2.bin" },
	{ "crimsoncrown woods",   "test/crimsoncrown/fixtures/woods.img",   "test/crimsoncrown/fixtures/woods.page",   true, true,  "test/crimsoncrown/fixtures/t2.bin" },
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
		if (img.empty() || gold.size() != kHiresPageSize) {
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
		for (int off = 0; off < kHiresPageSize; off++) {
			int row = hiresRowOfOffset(off);
			if (row < 0 || row >= kHiresPictureRows) continue;
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
				   c.name, diffs, firstOff, hiresRowOfOffset(firstOff),
				   page[firstOff], gold[firstOff]);
			failures++;
		}
	}

	printf("\n%d/%d cases passed\n", (int)(sizeof(kCases)/sizeof(kCases[0])) - failures,
		   (int)(sizeof(kCases)/sizeof(kCases[0])));
	return failures ? 1 : 0;
}

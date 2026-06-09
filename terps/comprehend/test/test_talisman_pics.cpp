/* test_talisman_pics.cpp -- regression test for the Apple II Talisman renderer.
 *
 * Each case pairs a picture's raw vector-opcode stream (extracted from the game
 * disk) with a golden Apple II hi-res page captured from MAME running the real
 * game. The test renders the stream through apple2_talisman.cpp and asserts the
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

#include "../apple2_talisman.h"
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

struct Case { const char *name; const char *img; const char *page; bool whiteBg; };

static const Case kCases[] = {
	// All three are bright (white-background) Talisman rooms on the Empire disk.
	// courtyard exercises the DRAW_BOX -> far-corner pen fix ($095b/$0990).
	{ "courtyard (RA #1)",    "test/talisman/courtyard.img",    "test/talisman/courtyard.page",    true },
	{ "executioner (RA #2)",  "test/talisman/executioner.img",  "test/talisman/executioner.page",  true },
	{ "cell (RA #4)",         "test/talisman/cell.img",         "test/talisman/cell.page",         true },
};

int main(int argc, char **argv) {
	// Allow running from the comprehend dir or its parent; prepend an optional
	// path prefix (argv[1]) so `make test` can pass the source dir.
	std::string prefix = (argc > 1) ? std::string(argv[1]) + "/" : "";

	// The renderer's drawing tables now come from the boot disk's T2 file;
	// load the captured copy so the test stays self-contained.
	std::vector<uint8_t> t2 = readFile(prefix + "test/talisman/t2.bin");
	if (t2.empty() || !talismanInstallDrawingTables(t2.data(), t2.size())) {
		fprintf(stderr, "FAIL: could not load test/talisman/t2.bin (size=%zu)\n",
			t2.size());
		return 1;
	}

	int failures = 0;
	for (const Case &c : kCases) {
		std::vector<uint8_t> img  = readFile(prefix + c.img);
		std::vector<uint8_t> gold = readFile(prefix + c.page);
		if (img.empty() || gold.size() != 0x2000) {
			fprintf(stderr, "FAIL %-22s : missing/short fixture (img=%zu page=%zu)\n",
				c.name, img.size(), gold.size());
			failures++;
			continue;
		}

		talismanResetScreen(c.whiteBg);
		talismanDrawImage(img.data(), img.size());
		const uint8_t *page = talismanPagePtr();

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

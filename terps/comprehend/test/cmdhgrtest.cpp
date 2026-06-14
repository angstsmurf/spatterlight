/* cmdhgrtest.cpp -- regression for The Coveted Mirror's double hi-res ("<D>")
 * room rendering, specifically op15/0 (the RLE-compressed bitmap rectangle that
 * CM room art is built from). Renders the throne room (RB image 1: op15/2 set
 * bounds, op15/3 fill, op15/0 RLE bitmap) with graphics_magician_dhgr.cpp and
 * asserts the DHGR aux+main pages are BYTE-EXACT over the picture region
 * (page byte offsets 0..20 of rows 0..159) versus the MAME ground truth.
 *
 * Self-contained: all fixtures are committed in test/cm_dhgr/
 *   T5.bin              -- the boot disk's <D> drawing tables
 *   throne_rb_idx1.bin  -- the throne room image stream (RB image 1, off..EOF)
 *   throne_main.page    -- MAME DHGR main page1 of the throne ($2000)
 *   throne_aux.page     -- MAME DHGR aux  page1 of the throne ($2000)
 *
 * The rightmost picture columns (21..23) are excluded: the captured page has
 * CM's right-hand panel composited over them, which the engine draws on a
 * separate pass (gmCaptureCMPanel/gmOverlayCMPanel), not part of the room image.
 *
 *   cmdhgrtest [fixture_dir]      (default: test/cm_dhgr)
 */
#include "../graphics_magician_dhgr.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Glk::Comprehend;
static int CALC(int y) { return (((y / 8) & 7) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10); }

static std::vector<uint8_t> rd(const char *p) {
	FILE *f = fopen(p, "rb");
	if (!f) { fprintf(stderr, "cannot open %s\n", p); exit(2); }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> v(n);
	if (fread(v.data(), 1, n, f) != (size_t)n) { fprintf(stderr, "short read %s\n", p); exit(2); }
	fclose(f); return v;
}

int main(int argc, char **argv) {
	const char *dir = (argc > 1) ? argv[1] : "test/cm_dhgr";
	char path[512];
	auto load = [&](const char *nm) { snprintf(path, sizeof path, "%s/%s", dir, nm); return rd(path); };

	auto t5 = load("T5.bin");
	if (!gmDhgrInstallDrawingTables(t5.data(), t5.size())) { fprintf(stderr, "no T5 tables\n"); return 2; }
	auto img = load("throne_rb_idx1.bin");
	auto gm = load("throne_main.page");
	auto ga = load("throne_aux.page");

	gmDhgrResetScreen(true);
	gmDhgrDrawImage(img.data(), img.size());
	const uint8_t *mm = gmDhgrMainPtr(), *ma = gmDhgrAuxPtr();

	int diff = 0;
	for (int y = 0; y < 160; y++) {
		int ba = CALC(y);
		for (int c = 0; c <= 20; c++) {
			int o = ba + c;
			if ((mm[o] & 0x7f) != (gm[o] & 0x7f)) { diff++; if (diff <= 20) printf("  diff row%d col%d main %02x/%02x\n", y, c, mm[o], gm[o]); }
			if ((ma[o] & 0x7f) != (ga[o] & 0x7f)) { diff++; if (diff <= 20) printf("  diff row%d col%d aux %02x/%02x\n", y, c, ma[o], ga[o]); }
		}
	}
	if (diff) { printf("FAIL: throne (RB idx1) op15/0 DHGR not byte-exact: %d differing bits\n", diff); return 1; }
	printf("PASS: throne (RB idx1) op15/0 DHGR byte-exact vs MAME (cols 0..20)\n");
	return 0;
}

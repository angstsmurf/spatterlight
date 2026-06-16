/* cmdhgrtest.cpp -- regression for The Coveted Mirror's double hi-res ("<D>")
 * room rendering, specifically op15/0 (the RLE-compressed bitmap rectangle that
 * CM room art is built from) and the flood-fill clip it establishes.
 *
 * Two cases, each asserting the DHGR aux+main pages are BYTE-EXACT over the
 * picture region (page byte offsets 0..20 of rows 0..159) vs the MAME ground
 * truth:
 *
 *   throne -- RB image 1 (op15/2 set bounds, op15/3 fill, op15/0 RLE bitmap),
 *             rendered standalone on a white screen.
 *   door   -- the Magician's Room open-cupboard scene: RA image 2 (room) then
 *             OE image 0 (item overlay) composited. OE0's only op15 is its op15/0
 *             RLE bitmap, which (like op15/2) sets the fill-clip rectangle to its
 *             bounds (rows 97..135); the following fill=62 PAINT must stay inside
 *             it and not leak onto the floor (DHGR col 25, rows 136..159). This
 *             guards the fill-leak fix in rle_bitmap_dhgr.
 *
 * Self-contained: all fixtures are committed in test/cm_dhgr/
 *   T5.bin              -- the boot disk's <D> drawing tables
 *   throne_rb_idx1.bin  -- the throne room image stream (RB image 1, off..EOF)
 *   throne_{main,aux}.page -- MAME DHGR main/aux page1 of the throne ($2000 each)
 *   door_ra2.bin        -- the Magician's Room image stream (RA image 2)
 *   door_oe0.bin        -- the open-cupboard overlay stream (OE image 0)
 *   door_{main,aux}.page -- MAME DHGR main/aux page1 of the composited scene
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

// Compare the live DHGR pages to the MAME ground-truth pages over the picture
// region (cols 0..20, rows 0..159). Returns the differing-byte count and prints
// up to 20 diffs.
static int comparePages(const uint8_t *gm, const uint8_t *ga) {
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
	return diff;
}

int main(int argc, char **argv) {
	const char *dir = (argc > 1) ? argv[1] : "test/cm_dhgr";
	char path[512];
	auto load = [&](const char *nm) { snprintf(path, sizeof path, "%s/%s", dir, nm); return rd(path); };

	auto t5 = load("T5.bin");
	if (!gmDhgrInstallDrawingTables(t5.data(), t5.size())) { fprintf(stderr, "no T5 tables\n"); return 2; }

	int fails = 0;

	// --- Case 1: throne room (RB idx1), standalone ----------------------------
	{
		auto img = load("throne_rb_idx1.bin");
		auto gm = load("throne_main.page");
		auto ga = load("throne_aux.page");
		gmDhgrResetScreen(true);
		gmDhgrDrawImage(img.data(), img.size());
		int diff = comparePages(gm.data(), ga.data());
		if (diff) { printf("FAIL: throne (RB idx1) op15/0 DHGR not byte-exact: %d differing bits\n", diff); fails++; }
		else printf("PASS: throne (RB idx1) op15/0 DHGR byte-exact vs MAME (cols 0..20)\n");
	}

	// --- Case 2: open-cupboard door (RA idx2 room + OE idx0 overlay) ----------
	{
		auto room = load("door_ra2.bin");
		auto item = load("door_oe0.bin");
		auto gm = load("door_main.page");
		auto ga = load("door_aux.page");
		gmDhgrResetScreen(true);
		gmDhgrDrawImage(room.data(), room.size());   // room base
		gmDhgrDrawImage(item.data(), item.size());   // item overlay, no reset
		int diff = comparePages(gm.data(), ga.data());
		if (diff) { printf("FAIL: door (RA idx2 + OE idx0) DHGR fill leak: %d differing bits\n", diff); fails++; }
		else printf("PASS: door (RA idx2 + OE idx0) op15/0 fill-clip byte-exact vs MAME (cols 0..20)\n");
	}

	return fails ? 1 : 0;
}

/* titledhgrtest.cpp -- regression for the Talisman double hi-res ("<D>") TITLE
 * image (t0), asserting the rendered DHGR aux+main pages are BYTE-EXACT over the
 * WHOLE picture region (page byte offsets 0..39, rows 0..159) vs the MAME ground
 * truth captured from a real apple2e.
 *
 * The title exercises the three <D> primitives the renderer was missing and that
 * this test guards against regressing:
 *   - op1/op3/op5 text glyphs  -- the "Challenging the Sands of Time" subtitle
 *     (font shares the brush-table base, glyph(ch)=T5[0xB2E+ch*8]).
 *   - op11 DRAW_CIRCLE          -- the lamp-knob circle; when it was a no-op the
 *     outline had a gap and the following fill=62 PAINT leaked into the whole sky.
 *   - the full 512-byte op6 sub-table -- high-subindex fills (fill colour 60 ->
 *     subindex 255 -> subtbl[510]) resolve to their real grey dither, not black.
 *
 * Unlike the Coveted Mirror cases (cmdhgrtest) there is no right-hand panel, so
 * the comparison covers every picture column (0..39) of both pages.
 *
 * Self-contained: all fixtures live (local-only, gitignored) in
 * test/title_dhgr/fixtures/ -- see test/title_dhgr/README.md for the recapture
 * recipe.
 *   T5.bin           -- the boot disk's <D> drawing tables (side 1 boot "T5")
 *   title_t0.bin     -- the Talisman title image stream (boot disk "t0", a raw
 *                       Graphics Magician vector stream from byte 0)
 *   title_{main,aux}.page -- MAME DHGR main/aux page1 ($2000 each) of the title
 *
 *   titledhgrtest [fixture_dir]      (default: test/title_dhgr/fixtures)
 */
#include "../graphics_magician_dhgr.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Glk::Comprehend;
static int CALC(int y) { return (((y / 8) & 7) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10); }

static std::vector<uint8_t> rd(const char *p, bool *ok) {
	FILE *f = fopen(p, "rb");
	if (!f) { *ok = false; return {}; }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> v(n);
	if (fread(v.data(), 1, n, f) != (size_t)n) { fclose(f); *ok = false; return {}; }
	fclose(f); *ok = true; return v;
}

// Compare the live DHGR pages to the MAME ground-truth pages over the full
// picture region (cols 0..39, rows 0..159). bit 7 is masked (ignored by the
// 560-wide DHGR display). Returns the differing-byte count, prints up to 20.
static int comparePages(const uint8_t *gm, const uint8_t *ga) {
	const uint8_t *mm = gmDhgrMainPtr(), *ma = gmDhgrAuxPtr();
	int diff = 0;
	for (int y = 0; y < 160; y++) {
		int ba = CALC(y);
		for (int c = 0; c <= 39; c++) {
			int o = ba + c;
			if ((mm[o] & 0x7f) != (gm[o] & 0x7f)) { diff++; if (diff <= 20) printf("  diff row%d col%d main %02x/%02x\n", y, c, mm[o], gm[o]); }
			if ((ma[o] & 0x7f) != (ga[o] & 0x7f)) { diff++; if (diff <= 20) printf("  diff row%d col%d aux %02x/%02x\n", y, c, ma[o], ga[o]); }
		}
	}
	return diff;
}

int main(int argc, char **argv) {
	const char *dir = (argc > 1) ? argv[1] : "test/title_dhgr/fixtures";
	char path[512];
	bool ok = true;
	auto load = [&](const char *nm) { snprintf(path, sizeof path, "%s/%s", dir, nm); bool o; auto v = rd(path, &o); ok = ok && o; return v; };

	auto t5    = load("T5.bin");
	auto img   = load("title_t0.bin");
	auto gmain = load("title_main.page");
	auto gaux  = load("title_aux.page");
	if (!ok) { printf("SKIP: title_dhgr fixtures not present in %s (local-only; see README)\n", dir); return 0; }

	if (!gmDhgrInstallDrawingTables(t5.data(), t5.size())) { fprintf(stderr, "no T5 tables\n"); return 2; }

	// The title is a single raw vector stream drawn onto a white-reset page
	// (op#1 op15/3 FILLRECT paints the 0x4d background itself).
	gmDhgrResetScreen(true);
	gmDhgrDrawImage(img.data(), img.size());

	int diff = comparePages(gmain.data(), gaux.data());
	if (diff) { printf("FAIL: Talisman title (t0) DHGR not byte-exact: %d differing bits\n", diff); return 1; }
	printf("PASS: Talisman title (t0) DHGR byte-exact vs MAME (text/circle/fill, cols 0..39)\n");
	return 0;
}

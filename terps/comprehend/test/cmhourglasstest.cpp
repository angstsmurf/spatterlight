/* cmhourglasstest.cpp -- regression for The Coveted Mirror hourglass logic that
 * is SHARED by the standard (graphics_magician.cpp) and double hi-res
 * (graphics_magician_dhgr.cpp) renderers: the per-grain pile geometry
 * (gmCMHourglassGrainPos, a byte-faithful port of cm_draw_hourglass_grain
 * $4347) and the per-turn drain arm / fall state machine (gmCMHourglassArm /
 * ...ConsumeFallArmed / ...Reset / ...FallBegin/Active/Step/Abort).
 *
 * These are pure-logic, fixture-free tests: the grain table is frozen against
 * the values the pixel-exact-validated port produces (validated vs the MAME
 * golden page test/cm/fixtures/throne_sand60.page -- see test/cm/README.md), and
 * the state machine is exercised directly. No drawing tables, disks or page
 * captures are required, so this runs in the default `make test`.
 *
 *   cmhourglasstest          run the asserts
 *   cmhourglasstest dump     print the grain table (regenerate the golden set)
 */
#include "../graphics_magician.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace Glk::Comprehend;

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: " __VA_ARGS__); printf("\n"); g_fail++; } } while (0)

// Golden grain table: {idx, present(0/1), x, y}. Frozen from gmCMHourglassGrainPos,
// the byte-faithful port of $4347, which renders pixel-exact vs the MAME
// throne_sand60.page capture. The structure (independently checked below) is:
//   - idx 0 is a no-op (the original treats grain 0 as nothing);
//   - the pile is centred at x=245, grains alternating right/left of it;
//   - grains form 25-wide bands, each band one screen row higher than the last.
struct Grain { int idx; int present; uint16_t x; uint8_t y; };
static const Grain kGolden[] = {
	{ 0, 0,   0,  0}, { 1, 1, 245, 92}, { 2, 1, 246, 91}, { 3, 1, 244, 91},
	{ 4, 1, 247, 90}, { 5, 1, 243, 90}, { 6, 1, 248, 89}, { 7, 1, 242, 89},
	{ 8, 1, 249, 88}, { 9, 1, 241, 88}, {10, 1, 250, 87}, {11, 1, 240, 87},
	{12, 1, 251, 86}, {13, 1, 239, 86}, {14, 1, 252, 85}, {15, 1, 238, 85},
	{16, 1, 253, 84}, {17, 1, 237, 84}, {18, 1, 254, 83}, {19, 1, 236, 83},
	{20, 1, 255, 82}, {21, 1, 235, 82}, {22, 1, 256, 81}, {23, 1, 234, 81},
	{24, 1, 257, 80}, {25, 1, 233, 80}, {26, 1, 245, 91}, {27, 1, 246, 90},
	{28, 1, 244, 90}, {29, 1, 247, 89}, {30, 1, 243, 89}, {31, 1, 248, 88},
	{32, 1, 242, 88}, {33, 1, 249, 87}, {34, 1, 241, 87}, {35, 1, 250, 86},
	{36, 1, 240, 86}, {37, 1, 251, 85}, {38, 1, 239, 85}, {39, 1, 252, 84},
	{40, 1, 238, 84}, {41, 1, 253, 83}, {42, 1, 237, 83}, {43, 1, 254, 82},
	{44, 1, 236, 82}, {45, 1, 255, 81}, {46, 1, 235, 81}, {47, 1, 256, 80},
	{48, 1, 234, 80}, {49, 1, 257, 79}, {50, 1, 233, 79}, {51, 1, 245, 90},
	{52, 1, 246, 89}, {53, 1, 244, 89}, {54, 1, 247, 88}, {55, 1, 243, 88},
	{56, 1, 248, 87}, {57, 1, 242, 87}, {58, 1, 249, 86}, {59, 1, 241, 86},
	{60, 1, 250, 85}, {61, 1, 240, 85}, {62, 1, 251, 84}, {63, 1, 239, 84},
	{64, 1, 252, 83}, {65, 1, 238, 83}, {66, 1, 253, 82}, {67, 1, 237, 82},
	{68, 1, 254, 81}, {69, 1, 236, 81}, {70, 1, 255, 80}, {71, 1, 235, 80},
	{72, 1, 256, 79}, {73, 1, 234, 79}, {74, 1, 257, 78}, {75, 1, 233, 78},
};
static const int kGoldenN = (int)(sizeof(kGolden) / sizeof(kGolden[0]));

static void dump_table() {
	for (int i = 0; i < kGoldenN; i++) {
		uint16_t x; uint8_t y;
		bool ok = gmCMHourglassGrainPos((uint8_t)i, &x, &y);
		printf("\t{%2d, %d, %3u, %3u},\n", i, ok ? 1 : 0, ok ? x : 0, ok ? y : 0);
	}
}

// TEST 1: every grain index matches the golden table exactly.
static void test_grain_positions() {
	for (int i = 0; i < kGoldenN; i++) {
		const Grain &g = kGolden[i];
		uint16_t x = 0xffff; uint8_t y = 0xff;
		bool ok = gmCMHourglassGrainPos((uint8_t)g.idx, &x, &y);
		CHECK(ok == (g.present != 0), "grain %d present=%d expected %d", g.idx, ok, g.present);
		if (!g.present)
			continue;
		CHECK(x == g.x && y == g.y, "grain %d at (%u,%u) expected (%u,%u)",
		      g.idx, x, y, g.x, g.y);
	}
}

// TEST 2: structural invariants derived independently of the frozen values
// (from cm/README.md + hourglass_anatomy.txt), so a wrong-but-self-consistent
// regression of the table is still caught.
static void test_grain_invariants() {
	uint16_t x; uint8_t y;
	// idx 0 is a no-op.
	CHECK(!gmCMHourglassGrainPos(0, &x, &y), "grain 0 must be a no-op");
	// grain 1 sits dead centre at the documented stream x=245.
	CHECK(gmCMHourglassGrainPos(1, &x, &y) && x == 245, "grain 1 must be centred at x=245");

	for (int i = 1; i <= 75; i++) {
		CHECK(gmCMHourglassGrainPos((uint8_t)i, &x, &y), "grain %d should be present", i);
		// Top-bulb triangle width (anatomy rows ~86-100): x within [233,257].
		CHECK(x >= 233 && x <= 257, "grain %d x=%u out of top-bulb range", i, x);
		// Plot-y climbs the bulb; the pile never reaches the waist (screen ~100).
		CHECK(y >= 78 && y <= 92, "grain %d y=%u out of top-bulb range", i, y);
	}
}

// TEST 3: the per-turn drain arm logic shared by both gmDrawCMHourglass() and
// gmDhgrDrawCMHourglass(). First draw snaps; a single-grain drop arms; a same-
// level repaint leaves a queued arm intact; any other jump snaps.
static void test_arm_state_machine() {
	gmCMHourglassReset();
	CHECK(!gmCMHourglassConsumeFallArmed(), "reset: nothing armed");

	gmCMHourglassArm(60);                 // first draw after reset -> snap
	CHECK(!gmCMHourglassConsumeFallArmed(), "first draw must snap, not arm");

	gmCMHourglassArm(59);                 // 60 -> 59: normal per-turn drop
	CHECK(gmCMHourglassConsumeFallArmed(), "single-grain drop must arm");
	CHECK(!gmCMHourglassConsumeFallArmed(), "consume clears the armed flag");

	// A same-level repaint (item overlay redraws the panel mid-turn) must not
	// clobber a fall armed earlier the same turn.
	gmCMHourglassArm(58);                 // 59 -> 58: arm
	gmCMHourglassArm(58);                 // same level: leave the armed flag alone
	CHECK(gmCMHourglassConsumeFallArmed(), "same-level repaint must preserve a queued arm");

	// Big jumps snap (bribe refill / catch / restore).
	gmCMHourglassArm(74);                 // 58 -> 74: jump up
	CHECK(!gmCMHourglassConsumeFallArmed(), "jump up must snap");
	gmCMHourglassArm(40);                 // 74 -> 40: jump down (> 1)
	CHECK(!gmCMHourglassConsumeFallArmed(), "multi-grain drop must snap");

	// Reset forgets the displayed level, so the next draw snaps again.
	gmCMHourglassArm(39);                 // 40 -> 39 would arm...
	CHECK(gmCMHourglassConsumeFallArmed(), "drop after jump arms");
	gmCMHourglassReset();
	gmCMHourglassArm(38);                 // ...but post-reset prev is -1 -> snap
	CHECK(!gmCMHourglassConsumeFallArmed(), "draw after reset must snap");
}

// TEST 4: the fall animation frame machine. gmCMHourglassFallStep returns true
// for each grain-bearing frame and false once on the trailing clean frame, after
// which the animation is inactive. (No panel is captured here, so the per-frame
// panel-band restore is a no-op; we are exercising the frame bookkeeping only.)
static void test_fall_frame_machine() {
	gmCMHourglassReset();
	CHECK(!gmCMHourglassFallActive(), "no fall active after reset");

	gmCMHourglassFallBegin();
	CHECK(gmCMHourglassFallActive(), "fall active after begin");

	int trueFrames = 0, y0 = 0, y1 = 0, guard = 0;
	while (gmCMHourglassFallStep(&y0, &y1)) {
		CHECK(y1 >= y0, "fall band must be non-empty (%d..%d)", y0, y1);
		if (++guard > 100) { CHECK(false, "fall never terminated"); break; }
		trueFrames++;
	}
	// 6 grain frames (kCMFallFrames) report `more`, the 7th (clean) returns false.
	CHECK(trueFrames == 6, "expected 6 grain frames, got %d", trueFrames);
	CHECK(!gmCMHourglassFallActive(), "fall inactive after it completes");

	// Abort from the middle snaps straight to inactive.
	gmCMHourglassFallBegin();
	gmCMHourglassFallStep(&y0, &y1);
	gmCMHourglassFallAbort();
	CHECK(!gmCMHourglassFallActive(), "fall inactive after abort");
}

// Apple II hi-res row base address (rows are address-interleaved). Mirrors the
// renderer's CALC_APPLE2_ADDRESS so the test can address the page directly.
static int row_base(int y) {
	return (((y / 8) & 7) << 7) + (((y / 8) & 0x18) * 5) + ((y & 7) << 10);
}

// TEST 5: the CM right-hand panel overlay (gmCaptureCMPanel + gmOverlayCMPanel).
// The panel columns (24..39) must be restored from the captured panel on top of
// the current room image, while the room columns (0..23) are left untouched. Uses
// the public gmSetPage/gmPagePtr page accessors, so it needs no disk fixtures.
static void test_panel_overlay() {
	static uint8_t pageA[0x2000], pageB[0x2000];
	std::memset(pageA, 0xAA, sizeof(pageA));   // "panel" content
	std::memset(pageB, 0x55, sizeof(pageB));   // "room" content

	gmSetPage(pageA);
	gmCaptureCMPanel(24, 39);                  // panel := pageA, columns 24..39
	CHECK(gmCMPanelValid(), "panel should be valid after capture");

	gmSetPage(pageB);                          // a fresh room is drawn
	gmOverlayCMPanel();                        // re-composite the saved panel

	const uint8_t *pg = gmPagePtr();
	const int rows[] = {0, 1, 95, 146, 191};
	for (int ri = 0; ri < (int)(sizeof(rows) / sizeof(rows[0])); ri++) {
		int base = row_base(rows[ri]);
		// Room columns survive untouched...
		for (int c = 0; c <= 23; c++)
			CHECK(pg[base + c] == 0x55, "room col %d row %d overwritten (%02x)", c, rows[ri], pg[base + c]);
		// ...panel columns are restored from the captured panel.
		for (int c = 24; c <= 39; c++)
			CHECK(pg[base + c] == 0xAA, "panel col %d row %d not restored (%02x)", c, rows[ri], pg[base + c]);
	}
}

// TEST 6: the CM panel must survive a *slow-draw reveal*. A room drawn with
// recording on appends every plotted byte -- including the panel columns 24..39 --
// to the reveal op list. gmOverlayCMPanel() then composites the saved panel onto
// both pages, but draining the reveal replays the recorded room bytes onto the
// slow page; without the panel-column skip (apple_apply_slow) those replays wipe
// the just-overlaid panel off the slow page mid-reveal. After a full drain the
// slow page's panel columns must hold the panel, not the recorded room content.
// (Mirrors the DHGR renderer, which already excludes the panel columns.)
static void test_panel_survives_reveal() {
	static uint8_t panelPage[0x2000];
	std::memset(panelPage, 0x2A, sizeof(panelPage));   // distinct "panel" content

	// 1. Capture the panel (columns 24..39) from a distinct page.
	gmSetPage(panelPage);
	gmCaptureCMPanel(24, 39);
	CHECK(gmCMPanelValid(), "panel should be valid after capture");

	// 2. Fresh black pages + op list, then record a full-width "room" reveal:
	//    horizontal white lines spanning all 40 columns at the sampled rows, so
	//    the op list contains writes in the panel columns 24..39 too.
	gmResetScreen(false);                              // pages := 0x00, ops cleared
	gmSetSlowDraw(true);
	const int rows[] = {0, 1, 95, 146, 191};
	const int nrows = (int)(sizeof(rows) / sizeof(rows[0]));
	uint8_t stream[1 + 6 * 5 + 1];
	int n = 0;
	stream[n++] = 0x23;                                // op2 SET_PEN_COLOR, white (0x7f)
	for (int i = 0; i < nrows; i++) {
		stream[n++] = 0x80; stream[n++] = 0x00; stream[n++] = (uint8_t)rows[i]; // MOVE_TO (0,y)
		stream[n++] = 0xA1; stream[n++] = 0x17; stream[n++] = (uint8_t)rows[i]; // DRAW_LINE (279,y)
	}
	stream[n++] = 0x00;                                // op0 END
	gmDrawImage(stream, n);

	// The just-recorded room content (the values the reveal will replay). Captured
	// before the overlay so the panel columns still hold the room's line bytes.
	static uint8_t roomDrawn[0x2000];
	std::memcpy(roomDrawn, gmPagePtr(), sizeof(roomDrawn));

	// Sanity: the room must actually differ from the panel in the panel columns,
	// or the test would pass vacuously even with the panel clobbered.
	bool distinct = false;
	for (int ri = 0; ri < nrows && !distinct; ri++) {
		int base = row_base(rows[ri]);
		for (int c = 24; c <= 39; c++)
			if (roomDrawn[base + c] != 0x2A) { distinct = true; break; }
	}
	CHECK(distinct, "test setup: recorded room must differ from panel in cols 24..39");

	// 3. Composite the panel, then drain the whole reveal.
	gmOverlayCMPanel();
	int guard = 0;
	while (gmAdvanceSlowDraw(100000)) {
		if (++guard > 1000) { CHECK(false, "reveal never drained"); break; }
	}

	// 4. Slow page: room columns reveal the room; panel columns keep the panel.
	const uint8_t *slow = gmSlowPagePtr();
	for (int ri = 0; ri < nrows; ri++) {
		int base = row_base(rows[ri]);
		for (int c = 0; c <= 23; c++)
			CHECK(slow[base + c] == roomDrawn[base + c],
			      "room col %d row %d not revealed (slow %02x != %02x)",
			      c, rows[ri], slow[base + c], roomDrawn[base + c]);
		for (int c = 24; c <= 39; c++)
			CHECK(slow[base + c] == 0x2A,
			      "panel col %d row %d clobbered by reveal (slow %02x != panel 2a)",
			      c, rows[ri], slow[base + c]);
	}

	gmSetSlowDraw(false);
}

int main(int argc, char **argv) {
	if (argc > 1 && std::strcmp(argv[1], "dump") == 0) {
		dump_table();
		return 0;
	}
	test_grain_positions();
	test_grain_invariants();
	test_arm_state_machine();
	test_fall_frame_machine();
	test_panel_overlay();
	test_panel_survives_reveal();
	if (g_fail) {
		printf("cmhourglasstest: %d FAILURES\n", g_fail);
		return 1;
	}
	printf("PASS: CM hourglass grain table (76 grains) + arm/fall state machines + panel overlay + slow-draw reveal\n");
	return 0;
}

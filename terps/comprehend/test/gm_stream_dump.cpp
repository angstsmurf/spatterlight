/* gm_stream_dump.cpp -- differential harness for the Graphics Magician opcode
 * interpreters.
 *
 * The game-picture fixtures are local-only, so a refactor of the opcode loops
 * (see TODO-graphics-magician-dispatcher.md) cannot rely on them.  This harness
 * instead renders deterministic pseudo-random but well-formed opcode streams
 * through ONE renderer with synthetic drawing tables, and prints a hash of the
 * final page, of every slow-draw reveal step and of every dirty band.  Build it
 * from a baseline checkout and from the working tree and `diff` the output:
 * any change in a plotted byte or in the reveal order shows up.
 *
 *   -DGMD_APPLE  Apple II hi-res (graphics_magician.cpp), talisman and legacy
 *   -DGMD_DHGR   double hi-res (graphics_magician_dhgr.cpp)
 *   -DGMD_CGA    DOS CGA, v2 and v1 built-in tables
 *   -DGMD_PCJR   DOS PCjr, synthetic JR_GRAPH tables
 *
 * Usage: gm_stream_dump [cases] [seed]     (default 300 cases, seed 1)
 * Run via test/gm_stream_diff.sh.
 *
 * Environment:
 *   GMD_NO_TRUNCATE=1  never cut a stream short (the baseline comparison: a
 *                      well-formed stream must render identically before and
 *                      after a change; a truncated one may not -- it used to read
 *                      past its buffer)
 *   GMD_EXACT=1        hand each stream to the renderer in a heap block of
 *                      EXACTLY its length, so AddressSanitizer flags any operand
 *                      read past the end (test/gm_stream_exact.sh).  The Apple
 *                      renderer is run without PAINT (and without its legacy
 *                      variant): its flood fill reads past its own page on some
 *                      streams, which is old behaviour and not about operands.
 *   GMD_VERBOSE=1      print each stream's bytes to stderr
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(GMD_APPLE)
#include "graphics_magician.h"
#elif defined(GMD_DHGR)
#include "graphics_magician.h"
#include "graphics_magician_dhgr.h"
#elif defined(GMD_CGA)
#include "graphics_magician_cga.h"
#elif defined(GMD_PCJR)
#include "graphics_magician_pcjr.h"
#else
#error "define one of GMD_APPLE GMD_DHGR GMD_CGA GMD_PCJR"
#endif

using namespace Glk::Comprehend;

// ---- deterministic RNG -----------------------------------------------------
static uint64_t s_rng = 1;
static uint32_t rnd() {
	s_rng = s_rng * 6364136223846793005ull + 1442695040888963407ull;
	return (uint32_t)(s_rng >> 33);
}
static int rn(int n) { return (int)(rnd() % (uint32_t)n); }

static uint64_t fnv(uint64_t h, const void *p, size_t n) {
	const uint8_t *b = (const uint8_t *)p;
	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 1099511628211ull;
	}
	return h;
}

// ---- stream generator ------------------------------------------------------
static void emitXY(std::vector<uint8_t> &s, int op, int x, int y) {
	s.push_back((uint8_t)((op << 4) | (x >= 256 ? 1 : 0)));
	s.push_back((uint8_t)(x & 0xff));
	s.push_back((uint8_t)y);
}

// `canFill15` keeps op15 sub-ops 2 away from renderers whose clip units we do not
// want to guess.
static std::vector<uint8_t> makeStream(bool allowOp7, bool allowRle, bool allowOp15) {
	std::vector<uint8_t> s;
	// Fill clip rectangle (source byte columns / rows) as the stream leaves it.  A
	// PAINT seed outside it is out-of-contract (the Apple span fill then reads past
	// its page), so seeds are drawn from inside.
	int cl = 0, cr = 39, ct = 0, cb = 159;
	int n = 4 + rn(40);
	for (int i = 0; i < n; i++) {
		int op = rn(16);
		int x = rn(280), y = rn(160);
		switch (op) {
		case 0:
			if (rn(8) == 0) s.push_back(0x00);        // early END
			break;
		case 1: emitXY(s, 1, x, y); break;
		case 2: s.push_back((uint8_t)(0x20 | rn(16))); break;
		case 3: case 5:
			s.push_back((uint8_t)(op << 4));
			s.push_back((uint8_t)(32 + rn(96)));
			break;
		case 4: s.push_back((uint8_t)(0x40 | rn(8))); break;
		case 6: s.push_back(0x60); s.push_back((uint8_t)rn(108)); break;
		case 7:
			if (allowOp7) {
				s.push_back(0x70); s.push_back((uint8_t)rn(256)); s.push_back((uint8_t)rn(256));
			}
			break;
		case 8: case 9: case 10: case 12: emitXY(s, op, x, y); break;
		case 11: s.push_back(0xb0); s.push_back((uint8_t)rn(40)); break;
		case 13: s.push_back(0xd0); s.push_back((uint8_t)rn(20)); break;
		case 14: {
#if defined(GMD_APPLE)
			if (getenv("GMD_EXACT")) break;     // Apple fill over-reads its page (see above)
#endif
			int px = cl * 7 + rn((cr - cl + 1) * 7), py = ct + rn(cb - ct + 1);
			emitXY(s, 14, px, py);
			break;
		}
		case 15: {
			if (!allowOp15) break;
			int k = rn(8);
			if (k < 3) { s.push_back(0xf1); cl = 0; cr = 39; ct = 0; cb = 159; }
			else if (k < 5) s.push_back(0xf3);
			else if (k < 7) {
				int l = rn(30), r = l + rn(40 - l), t = rn(100), b = t + rn(160 - t);
				s.push_back(0xf2);
				cl = l; cr = r; ct = t; cb = b;
				s.push_back((uint8_t)r); s.push_back((uint8_t)l);
				s.push_back((uint8_t)b); s.push_back((uint8_t)t);
			} else if (allowRle) {
				int l = rn(30), r = l + rn(8), t = rn(100), b = t + rn(40);
				s.push_back(0xf0);
				cl = l; cr = r; ct = t; cb = b;
				s.push_back((uint8_t)r); s.push_back((uint8_t)l);
				s.push_back((uint8_t)b); s.push_back((uint8_t)t);
				int cells = (r - l + 1) * (b - t + 1);
				for (int c = 0; c < cells; c++) {
					if (rn(6) == 0) {
						s.push_back(0x80); s.push_back((uint8_t)rn(6)); s.push_back((uint8_t)rn(256));
					} else {
						uint8_t v = (uint8_t)rn(256);
						s.push_back(v == 0x80 ? 0x81 : v);
					}
				}
			}
			break;
		}
		}
	}
	if (rn(2)) s.push_back(0x00);
	return s;
}

// ---- renderer bindings -----------------------------------------------------
struct Bind {
	void (*reset)(bool);
	void (*draw)(const uint8_t *, size_t);
	void (*setSlow)(bool);
	bool (*slowActive)();
	bool (*advance)(int);
	void (*finish)();
	bool (*dirty)(int *, int *);
	void (*blit)(uint32_t *, int, int);
	void (*blitSlow)(uint32_t *, int, int);
	int w, h;
};

#if defined(GMD_APPLE)
static const Bind kBind = { gmResetScreen, gmDrawImage, gmSetSlowDraw, gmSlowDrawActive,
	gmAdvanceSlowDraw, gmFinishSlowDraw, gmSlowConsumeDirty, gmBlitToSurface,
	gmBlitSlowToSurface, 560, 192 };
#elif defined(GMD_DHGR)
static const Bind kBind = { gmDhgrResetScreen, gmDhgrDrawImage, gmDhgrSetSlowDraw,
	gmDhgrSlowDrawActive, gmDhgrAdvanceSlowDraw, gmDhgrFinishSlowDraw,
	gmDhgrSlowConsumeDirty, gmDhgrBlitToSurface, gmDhgrBlitSlowToSurface, 560, 192 };
#elif defined(GMD_CGA)
static const Bind kBind = { gmcgaResetScreen, gmcgaDrawImage, gmcgaSetSlowDraw,
	gmcgaSlowDrawActive, gmcgaAdvanceSlowDraw, gmcgaFinishSlowDraw,
	gmcgaSlowConsumeDirty, gmcgaBlitToSurface, gmcgaBlitSlowToSurface, 280, 160 };
#else
static const Bind kBind = { gmpcjrResetScreen, gmpcjrDrawImage, gmpcjrSetSlowDraw,
	gmpcjrSlowDrawActive, gmpcjrAdvanceSlowDraw, gmpcjrFinishSlowDraw,
	gmpcjrSlowConsumeDirty, gmpcjrBlitToSurface, gmpcjrBlitSlowToSurface, 280, 160 };
#endif

// ---- Coveted Mirror panel (Apple pair only) -------------------------------
// Capture / overlay / rebuild / hourglass / grain fall, interleaved with the
// drawing so the slow reveal's panel-column skipping and the page copies are
// exercised too.  Everything it observes goes into g_cmHash.
static uint64_t g_cmHash = 14695981039346656037ull;
#if defined(GMD_APPLE) || defined(GMD_DHGR)
#include "cm_panel.h"
#if defined(GMD_APPLE)
#define CM(name) gm##name
#define CM_FALL(name) gmCMHourglass##name
#else
#define CM(name) gmDhgr##name
#define CM_FALL(name) gmDhgrCMHourglass##name
#endif
static int s_cmSand = 40;
static void cmMix(const void *p, size_t n) { g_cmHash = fnv(g_cmHash, p, n); }
static void cmOps() {
	switch (rn(7)) {
	case 0: CM(CaptureCMPanel)(24, 39); break;
	case 1: CM(OverlayCMPanel)(); break;
	case 2: s_cmSand = rn(77); CM(DrawCMHourglass)(s_cmSand); break;
	case 3: if (s_cmSand > 0) s_cmSand--; CM(DrawCMHourglass)(s_cmSand); break;
	case 4: {
		CM(BeginCMPanelRebuild)();
		std::vector<uint8_t> t = makeStream(false, false, false);
		t.resize(t.size() + 8, 0);
		kBind.draw(t.data(), t.size() - 8);
		CM(EndCMPanelRebuild)();
		break;
	}
	case 5: {
		bool armed = gmCMHourglassConsumeFallArmed();
		cmMix(&armed, sizeof(armed));
		if (armed) {
			CM_FALL(FallBegin)();
			for (int i = 0; i < 12 && CM_FALL(FallActive)(); i++) {
				int y0 = -1, y1 = -1;
				bool more = CM_FALL(FallStep)(&y0, &y1);
				cmMix(&more, sizeof(more)); cmMix(&y0, sizeof(y0)); cmMix(&y1, sizeof(y1));
				if (rn(9) == 0) { CM_FALL(FallAbort)(); break; }
			}
		}
		break;
	}
	case 6: gmCMHourglassReset(); break;
	}
	bool valid = CM(CMPanelValid)();
	cmMix(&valid, sizeof(valid));
}
#endif

// ---- synthetic drawing tables ---------------------------------------------
static void fillRandom(uint8_t *p, size_t n) {
	for (size_t i = 0; i < n; i++) p[i] = (uint8_t)rnd();
}

static bool installTables(int variant) {
	(void)variant;
#if defined(GMD_APPLE)
	// [216 sub-indices < 30][4 zero][signature][rest of pattern][1024 font/brush]
	static const uint8_t sig[] = {
		0x55,0x2a,0x55,0x2a, 0x2a,0x55,0x2a,0x55, 0x7f,0x7f,0x7f,0x7f,
		0x80,0x80,0x80,0x80, 0xd5,0xaa,0xd5,0xaa, 0xaa,0xd5,0xaa,0xd5,
		0xff,0xff,0xff,0xff,
	};
	std::vector<uint8_t> t2(216 + 120 + 1024);
	for (int i = 0; i < 216; i++) t2[i] = (uint8_t)rn(30);
	memset(&t2[216], 0, 4);
	memcpy(&t2[220], sig, sizeof(sig));
	fillRandom(&t2[220 + sizeof(sig)], 120 - 4 - sizeof(sig));
	fillRandom(&t2[336], 1024);
	return gmInstallDrawingTables(t2.data(), t2.size());
#elif defined(GMD_DHGR)
	std::vector<uint8_t> t5(0x1000);
	fillRandom(t5.data(), t5.size());
	return gmDhgrInstallDrawingTables(t5.data(), t5.size());
#elif defined(GMD_CGA)
	if (variant == 0) {
		gmcgaInstallBuiltinDrawingTables(false);
		return true;
	}
	{   // v1: synthetic PC_GRAPH.OVR + NOVEL.EXE + CHARSET.GDA through the real loaders
		static const uint8_t kPatSig[] = {
			0x00,0x00,0x00,0x00, 0xaa,0xaa,0xaa,0xaa,
			0x55,0x55,0x55,0x55, 0xff,0xff,0xff,0xff,
		};
		static const uint8_t kBrush5Sig[] = {
			0x00,0x03,0x1f,0x3f,0x3f,0x3f,0x7f,0x7f,
			0x7f,0x7f,0x3f,0x3f,0x3f,0x1f,0x03,0x00,
		};
		std::vector<uint8_t> ovr(32 + 120 + 218 + 16);
		fillRandom(ovr.data(), ovr.size());
		memcpy(&ovr[32], kPatSig, sizeof(kPatSig));
		for (int i = 0; i < 218; i++) ovr[32 + 120 + i] = (uint8_t)rn(30);
		std::vector<uint8_t> exe(1024);
		fillRandom(exe.data(), exe.size());
		memcpy(&exe[300 + 5 * 32], kBrush5Sig, sizeof(kBrush5Sig));
		if (!gmcgaInstallV1DrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size()))
			return false;
		std::vector<uint8_t> gda(4 + 96 * 8);
		fillRandom(gda.data(), gda.size());
		gda[0] = 0x00; gda[1] = 0x11;
		return gmcgaSetV1Font(gda.data(), gda.size());
	}
#else
	static const uint8_t kSig[] = { 0x00, 0x20, 0x0f, 0xf0 };
	static const uint8_t kBrush5Sig[] = {
		0x00,0x03,0x1f,0x3f,0x3f,0x3f,0x7f,0x7f,
		0x7f,0x7f,0x3f,0x3f,0x3f,0x1f,0x03,0x00,
	};
	std::vector<uint8_t> ovr(16 + 4 + 0xa3 + 216 + 16);
	fillRandom(ovr.data(), ovr.size());
	memcpy(&ovr[16], kSig, 4);
	std::vector<uint8_t> exe(1024);
	fillRandom(exe.data(), exe.size());
	memcpy(&exe[200 + 5 * 32], kBrush5Sig, sizeof(kBrush5Sig));
	if (!gmpcjrInstallDrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size()))
		return false;
	std::vector<uint8_t> gda(4 + 96 * 8);
	fillRandom(gda.data(), gda.size());
	gda[0] = 0x00; gda[1] = 0x11;
	return gmpcjrSetFont(gda.data(), gda.size());
#endif
}

// ---- one case --------------------------------------------------------------
static uint64_t hashBlit(uint64_t h, void (*blit)(uint32_t *, int, int), int w, int hgt) {
	static std::vector<uint32_t> buf;
	buf.assign((size_t)w * hgt, 0);
	blit(buf.data(), w, hgt);
	return fnv(h, buf.data(), buf.size() * sizeof(uint32_t));
}

static void runCase(int idx, int variant, bool slow) {
	const bool white = rn(2);
#if defined(GMD_APPLE) || defined(GMD_DHGR)
	gmCMHourglassReset();
	g_cmHash = 14695981039346656037ull;
#endif
	kBind.reset(white);
	kBind.setSlow(slow);
	uint64_t slowHash = 14695981039346656037ull;
	for (int img = 0; img < 2; img++) {          // room, then an item overlay
#if defined(GMD_APPLE)
		const bool legacy = variant == 1;      // legacy dialect: op7 / op15 end the image
#else
		const bool legacy = false;
#endif
		bool allowRle = !legacy;
#if defined(GMD_APPLE)
		if (getenv("GMD_EXACT"))
			allowRle = false;     // leftover RLE bytes would run as ops, incl. PAINT
#endif
		std::vector<uint8_t> s = makeStream(!legacy, allowRle, !legacy);
		// Pad so operand reads that run off a truncated tail stay inside the buffer.
		size_t size = s.size();
		if (rn(6) == 0 && size > 2) {
			const size_t cut = 1 + rn(2);          // deliberate truncation
			if (!getenv("GMD_NO_TRUNCATE"))
				size -= cut;
		}
		s.resize(s.size() + 8, 0);
		if (getenv("GMD_VERBOSE")) {
			fprintf(stderr, "case %d v%d s%d img%d:", idx, variant, slow ? 1 : 0, img);
			for (size_t i = 0; i < size; i++) fprintf(stderr, " %02x", s[i]);
			fprintf(stderr, "\n");
		}
#if defined(GMD_APPLE) || defined(GMD_DHGR)
		if (!getenv("GMD_EXACT"))
			for (int k = rn(3); k > 0; k--)
				cmOps();
#endif
		if (getenv("GMD_EXACT")) {
			uint8_t *exact = new uint8_t[size ? size : 1];
			memcpy(exact, s.data(), size);
			kBind.draw(exact, size);
			delete[] exact;
		} else {
			kBind.draw(s.data(), size);
		}
	}
	if (slow) {
		int guard = 0;
		while (kBind.slowActive() && guard++ < 100000) {
			bool more = kBind.advance(1 + rn(40));
			int y0 = -1, y1 = -1;
			bool d = kBind.dirty(&y0, &y1);
			slowHash = fnv(slowHash, &more, sizeof(more));
			slowHash = fnv(slowHash, &d, sizeof(d));
			slowHash = fnv(slowHash, &y0, sizeof(y0));
			slowHash = fnv(slowHash, &y1, sizeof(y1));
			if (guard % 7 == 0)
				slowHash = hashBlit(slowHash, kBind.blitSlow, kBind.w, kBind.h);
		}
		slowHash = hashBlit(slowHash, kBind.blitSlow, kBind.w, kBind.h);
	}
	uint64_t fin = hashBlit(14695981039346656037ull, kBind.blit, kBind.w, kBind.h);
#if defined(GMD_APPLE)
	fin = fnv(fin, gmPagePtr(), 0x2000);
#elif defined(GMD_DHGR)
	fin = fnv(fin, gmDhgrMainPtr(), 0x2000);
	fin = fnv(fin, gmDhgrAuxPtr(), 0x2000);
#endif
	printf("%d v%d s%d %016llx %016llx %016llx\n", idx, variant, slow ? 1 : 0,
		   (unsigned long long)fin, (unsigned long long)slowHash,
		   (unsigned long long)g_cmHash);
}

int main(int argc, char **argv) {
	int cases = argc > 1 ? atoi(argv[1]) : 300;
	uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 1;
	s_rng = seed * 0x9e3779b97f4a7c15ull + 12345;

	// Variants: Apple = 0 Talisman / 1 legacy; CGA = 0 v2 / 1 v1; others unused.
	for (int variant = 0; variant < 2; variant++) {
#if defined(GMD_DHGR) || defined(GMD_PCJR)
		if (variant == 1) break;
#endif
#if defined(GMD_APPLE)
		if (variant == 1 && getenv("GMD_EXACT")) break;
#endif
		if (!installTables(variant)) {
			fprintf(stderr, "table install failed\n");
			return 2;
		}
#if defined(GMD_APPLE)
		gmSetLegacyFormat(variant == 1);
#endif
		for (int i = 0; i < cases; i++) {
			runCase(i, variant, false);
			runCase(i, variant, true);
		}
	}
	return 0;
}

/* pixtest.cpp -- offline harness to log every hi-res write the Talisman
 * standard hi-res renderer makes while drawing one picture, so it can be
 * diffed against MAME's writes to the real Apple II hi-res page.
 *
 * Usage: pixtest <disk.woz> <FILE> <index> [out.log]
 *   FILE  = picture file name as stored on disk (RA..RG room, OA..OF item)
 *   index = image index 0..15 within that file
 *
 * Prints one "offset value" line per write (hex), in draw order, to stdout
 * (or out.log). offset is the byte offset within the 0x2000 hi-res page, i.e.
 * MAME address minus the page base ($2000 or $4000).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

extern "C" {
#include "../../readwoz/ciderpress.h"
#include "../../readwoz/woz2nib.h"
}

namespace Glk { namespace Comprehend {
void gmResetScreen(bool white);
void gmDrawImage(const uint8_t *data, size_t size);
const uint8_t *gmPagePtr();
void gmSetPage(const uint8_t *p);
void gmBlitToSurface(uint32_t *out, int w, int h);
bool gmInstallDrawingTables(const uint8_t *t2, size_t size);
void gmDrawCMHourglass(int sand);
void gmCaptureCMPanel(int col0, int col1);
void gmOverlayCMPanel();
void gmSetSlowDraw(bool on);
bool gmSlowDrawActive();
bool gmAdvanceSlowDraw(int budget);
void gmFinishSlowDraw();
void gmBlitSlowToSurface(uint32_t *out, int w, int h);
bool gmCMHourglassConsumeFallArmed();
void gmCMHourglassFallBegin();
bool gmCMHourglassFallActive();
bool gmCMHourglassFallStep(int *y0, int *y1);
// Double hi-res ("<D>") renderer + Coveted Mirror panel.
bool gmDhgrInstallDrawingTables(const uint8_t *t5, size_t size);
void gmDhgrResetScreen(bool white);
void gmDhgrDrawImage(const uint8_t *data, size_t size);
void gmDhgrBlitToSurface(uint32_t *out, int w, int h);
void gmDhgrCaptureCMPanel(int col0, int col1);
void gmDhgrOverlayCMPanel();
void gmDhgrDrawCMHourglass(int sand);
void gmDhgrSetSlowDraw(bool on);
bool gmDhgrSlowDrawActive();
bool gmDhgrAdvanceSlowDraw(int budget);
void gmDhgrFinishSlowDraw();
void gmDhgrBlitSlowToSurface(uint32_t *out, int w, int h);
extern void (*g_gmWriteLog)(uint16_t, uint8_t, char);
extern void (*g_gmOnOp)(int pos, int op, int b1, int b2, int x, int y);
}}

using namespace Glk::Comprehend;

#define DSK_IMAGE_SIZE (35 * 16 * 256)
#define NIB_IMAGE_SIZE (35 * 6656)

static FILE *g_out = nullptr;
static long g_count = 0;
static FILE *g_optrace = nullptr;
static void logWrite(uint16_t off, uint8_t val, char prim) {
	fprintf(g_out, "%04x %02x %c\n", off, val, prim);
	g_count++;
}
static const char *opName(int op) {
	static const char *n[16] = {"END","TEXTPOS","PEN","CHAR","SHAPE","OUTLINE","FILLCOL","END2",
		"MOVE","BOX","LINE","CIRCLE","DRAWSHP","DELAY","PAINT","RESET"};
	return n[(op >> 4) & 0xf];
}
static void onOp(int pos, int op, int b1, int b2, int x, int y) {
	if (g_optrace)
		fprintf(g_optrace, "wr=%ld pos=%d op=%02x(%s.%d) b1=%d b2=%d penxy=(%d,%d)\n",
			g_count, pos, op, opName(op), op & 0xf, b1, b2, x, y);
}

static uint8_t *readFile(const char *path, size_t *sz) {
	FILE *f = fopen(path, "rb");
	if (!f) return nullptr;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	uint8_t *d = (uint8_t *)malloc(n);
	if (fread(d, 1, n, f) != (size_t)n) { free(d); fclose(f); return nullptr; }
	fclose(f); *sz = n; return d;
}

static uint8_t *toDsk(uint8_t *data, size_t size, size_t *outSize) {
	if (size >= 4 && data[0]=='W' && data[1]=='O' && data[2]=='Z') {
		size_t nibSize = size;
		uint8_t *nib = woz2nib(data, &nibSize);
		if (!nib) return nullptr;
		uint8_t *dsk = ReadFullImageFromNib(nib, nibSize);
		free(nib);
		*outSize = DSK_IMAGE_SIZE;
		return dsk;
	}
	if (size == NIB_IMAGE_SIZE) { *outSize = DSK_IMAGE_SIZE; return ReadFullImageFromNib(data, size); }
	if (size >= DSK_IMAGE_SIZE) { uint8_t *d=(uint8_t*)malloc(DSK_IMAGE_SIZE); memcpy(d,data,DSK_IMAGE_SIZE); *outSize=DSK_IMAGE_SIZE; return d; }
	return nullptr;
}

static void renderPageToPPM(const char *pageIn, const char *ppmOut) {
	FILE *f = fopen(pageIn, "rb");
	uint8_t page[0x2000];
	if (!f || fread(page, 1, 0x2000, f) != 0x2000) { fprintf(stderr, "bad page file\n"); return; }
	fclose(f);
	gmSetPage(page);
	int w = 280, h = 192;
	uint32_t *rgba = (uint32_t *)malloc(w * h * 4);
	gmBlitToSurface(rgba, w, h);
	FILE *o = fopen(ppmOut, "wb");
	fprintf(o, "P6\n%d %d\n255\n", w, h);
	for (int i = 0; i < w * h; i++) {
		uint32_t p = rgba[i];
		uint8_t rgb[3] = { (uint8_t)(p >> 24), (uint8_t)(p >> 16), (uint8_t)(p >> 8) };
		fwrite(rgb, 1, 3, o);
	}
	fclose(o);
	free(rgba);
	fprintf(stderr, "rendered %s -> %s\n", pageIn, ppmOut);
}

int main(int argc, char **argv) {
	if (argc >= 4 && strcmp(argv[1], "--render") == 0) { renderPageToPPM(argv[2], argv[3]); return 0; }
	if (argc < 4) { fprintf(stderr, "usage: %s <disk.woz> <FILE> <index> [out.log]\n", argv[0]); return 2; }
	const char *diskPath = argv[1];
	const char *wantName = argv[2];
	int index = atoi(argv[3]);
	g_out = (argc >= 5) ? fopen(argv[4], "w") : stdout;
	if (!g_out) { fprintf(stderr, "cannot open output\n"); return 2; }

	size_t rawSize = 0;
	uint8_t *raw = readFile(diskPath, &rawSize);
	if (!raw) { fprintf(stderr, "cannot read %s\n", diskPath); return 1; }
	size_t dskSize = 0;
	uint8_t *dsk = toDsk(raw, rawSize, &dskSize);
	free(raw);
	if (!dsk) { fprintf(stderr, "cannot decode disk image\n"); return 1; }

	size_t numFiles = 0;
	A2FileRec *files = GetAllProDOSFiles(dsk, dskSize, &numFiles);
	if (!files || numFiles == 0) { fprintf(stderr, "no files extracted\n"); return 1; }

	A2FileRec *pic = nullptr;
	A2FileRec *t2 = nullptr;
	for (size_t i = 0; i < numFiles; i++) {
		if (strcasecmp(files[i].filename, wantName) == 0) pic = &files[i];
		if (strcasecmp(files[i].filename, "T2") == 0)     t2  = &files[i];
	}
	if (!pic) {
		fprintf(stderr, "file '%s' not on disk. Available:", wantName);
		for (size_t i = 0; i < numFiles; i++) fprintf(stderr, " %s", files[i].filename);
		fprintf(stderr, "\n");
		return 1;
	}
	if (t2) {
		gmInstallDrawingTables(t2->data, t2->datasize);
	} else {
		// Side 2/3 don't carry T2; fall back to the captured fixture so the
		// renderer still has its drawing tables.
		size_t fxSz = 0;
		uint8_t *fx = readFile("test/talisman/fixtures/t2.bin", &fxSz);
		if (!fx) fx = readFile("terps/comprehend/test/talisman/fixtures/t2.bin", &fxSz);
		if (fx && gmInstallDrawingTables(fx, fxSz)) {
			fprintf(stderr, "loaded drawing tables from test/talisman/fixtures/t2.bin\n");
		} else {
			fprintf(stderr, "warning: 'T2' not on this disk and no t2.bin fixture; drawing tables left zero\n");
		}
		free(fx);
	}

	// CM_LIVE=<roomFile>:<idx>:<sand>[:slow] -- replicate pics.cpp's Coveted
	// Mirror branch exactly (lazy panel build = RG0 logo + OG0 frame captured at
	// cols 24-39, then room render, then overlay panel, then stamp the pile),
	// optionally driving the slow-draw reveal to completion, and dump the final
	// page to PIXTEST_PAGE. Used to reproduce the live composite offline.
	const char *live = getenv("CM_LIVE");
	if (live) {
		auto findFile = [&](const char *nm) -> A2FileRec * {
			for (size_t i = 0; i < numFiles; i++)
				if (strcasecmp(files[i].filename, nm) == 0) return &files[i];
			return nullptr;
		};
		auto renderImg = [&](A2FileRec *f, int idx) {
			const uint8_t *b = f->data;
			uint16_t ver = b[0] | (b[1] << 8);
			size_t bs = (ver == 0x1000) ? 4 : 0;
			size_t p = bs + idx * 2;
			uint16_t st = b[p] | (b[p + 1] << 8);
			if (ver == 0x1000) st += 4;
			gmDrawImage(b + st, f->datasize - st);
		};
		char buf[64]; strncpy(buf, live, sizeof(buf) - 1); buf[63] = 0;
		char *roomNm = strtok(buf, ":");
		int roomIdx = atoi(strtok(nullptr, ":") ?: "0");
		const char *sandS = strtok(nullptr, ":"); int sand = sandS ? atoi(sandS) : 74;
		const char *slowS = strtok(nullptr, ":"); bool slow = slowS && atoi(slowS);
		A2FileRec *RG = findFile("RG"), *OG = findFile("OG"), *RM = findFile(roomNm);
		if (!RG || !OG || !RM) { fprintf(stderr, "CM_LIVE: missing RG/OG/%s\n", roomNm); return 1; }
		gmSetSlowDraw(slow);
		// lazy panel build (pics.cpp ~611-617)
		gmResetScreen(false);
		renderImg(RG, 0);
		renderImg(OG, 0);
		gmCaptureCMPanel(24, 39);
		// actual room (clearBg => white reset), then overlay + hourglass
		gmResetScreen(true);
		renderImg(RM, roomIdx);
		gmOverlayCMPanel();
		gmDrawCMHourglass(sand);
		if (slow) { while (gmSlowDrawActive()) gmAdvanceSlowDraw(64); gmFinishSlowDraw(); }
		// CM_LIVE_FALL=N: arm a one-grain drop and step the fall to frame N, the
		// way tickHourglass() does each per-turn drop, to dump the falling grain
		// composited over the real room.
		const char *fallN = getenv("CM_LIVE_FALL");
		if (fallN) {
			gmOverlayCMPanel();
			gmDrawCMHourglass(sand - 1);   // drop one -> arms the fall
			gmCMHourglassConsumeFallArmed();
			gmCMHourglassFallBegin();
			int want = atoi(fallN), y0, y1;
			for (int f = 0; f <= want && gmCMHourglassFallActive(); f++)
				gmCMHourglassFallStep(&y0, &y1);
		}
		const char *pp = getenv("PIXTEST_PAGE");
		if (pp) { FILE *pf = fopen(pp, "wb"); if (pf) { fwrite(gmPagePtr(), 1, 0x2000, pf); fclose(pf); } }
		fprintf(stderr, "CM_LIVE %s idx%d sand%d slow%d done\n", roomNm, roomIdx, sand, slow);
		return 0;
	}

	// CM_LIVE_DHGR=<roomFile>:<idx>:<sand> -- the double-hi-res counterpart of
	// CM_LIVE: build the DHGR panel (RG0 logo + OG0 frame, captured cols 24-39 on
	// the aux+main pages), render the room, overlay the panel + stamp the pile,
	// then write the 560-wide NTSC composite to PIXTEST_PPM. Confirms the panel
	// shows in double hi-res. Needs the boot disk's T5 tables (file or fixture).
	const char *liveD = getenv("CM_LIVE_DHGR");
	if (liveD) {
		A2FileRec *t5 = nullptr;
		for (size_t i = 0; i < numFiles; i++)
			if (strcasecmp(files[i].filename, "T5") == 0) t5 = &files[i];
		bool haveT5 = t5 && gmDhgrInstallDrawingTables(t5->data, t5->datasize);
		if (!haveT5) {
			size_t fxSz = 0;
			uint8_t *fx = readFile("test/cm_dhgr/fixtures/T5.bin", &fxSz);
			if (!fx) fx = readFile("terps/comprehend/test/cm_dhgr/fixtures/T5.bin", &fxSz);
			haveT5 = fx && gmDhgrInstallDrawingTables(fx, fxSz);
			free(fx);
		}
		if (!haveT5) { fprintf(stderr, "CM_LIVE_DHGR: no T5 tables\n"); return 1; }

		auto findFile = [&](const char *nm) -> A2FileRec * {
			for (size_t i = 0; i < numFiles; i++)
				if (strcasecmp(files[i].filename, nm) == 0) return &files[i];
			return nullptr;
		};
		auto renderImgD = [&](A2FileRec *f, int idx) {
			const uint8_t *b = f->data;
			uint16_t ver = b[0] | (b[1] << 8);
			size_t bs = (ver == 0x1000) ? 4 : 0;
			size_t p = bs + idx * 2;
			uint16_t st = b[p] | (b[p + 1] << 8);
			if (ver == 0x1000) st += 4;
			gmDhgrDrawImage(b + st, f->datasize - st);
		};
		char buf[64]; strncpy(buf, liveD, sizeof(buf) - 1); buf[63] = 0;
		char *roomNm = strtok(buf, ":");
		int roomIdx = atoi(strtok(nullptr, ":") ?: "0");
		const char *sandS = strtok(nullptr, ":"); int sand = sandS ? atoi(sandS) : 74;
		const char *slowS = strtok(nullptr, ":"); bool slow = slowS && atoi(slowS);
		A2FileRec *RG = findFile("RG"), *OG = findFile("OG"), *RM = findFile(roomNm);
		if (!RG || !OG || !RM) { fprintf(stderr, "CM_LIVE_DHGR: missing RG/OG/%s\n", roomNm); return 1; }
		// lazy panel build (pics.cpp): logo RG0 + hourglass OG0, captured cols 24-39
		gmDhgrSetSlowDraw(slow);
		gmDhgrResetScreen(false);
		renderImgD(RG, 0);
		renderImgD(OG, 0);
		gmDhgrCaptureCMPanel(24, 39);
		// the actual room, then overlay panel + stamp the pile
		gmDhgrResetScreen(true);
		renderImgD(RM, roomIdx);
		gmDhgrOverlayCMPanel();
		gmDhgrDrawCMHourglass(sand);
		// Drive the reveal to completion and dump the SLOW (progressively revealed)
		// pages: this is what the host blits during a room change, where the panel
		// must survive the full-width op15/0 reveal.
		bool dumpSlow = slow;
		if (slow) { while (gmDhgrSlowDrawActive()) gmDhgrAdvanceSlowDraw(64); gmDhgrFinishSlowDraw(); }
		const char *ppm = getenv("PIXTEST_PPM");
		if (ppm) {
			int w = 560, h = 192;
			uint32_t *rgba = (uint32_t *)malloc(w * h * 4);
			if (dumpSlow) gmDhgrBlitSlowToSurface(rgba, w, h);
			else gmDhgrBlitToSurface(rgba, w, h);
			FILE *o = fopen(ppm, "wb");
			fprintf(o, "P6\n%d %d\n255\n", w, h);
			for (int i = 0; i < w * h; i++) {
				uint32_t px = rgba[i];
				uint8_t rgb[3] = { (uint8_t)(px >> 24), (uint8_t)(px >> 16), (uint8_t)(px >> 8) };
				fwrite(rgb, 1, 3, o);
			}
			fclose(o); free(rgba);
		}
		fprintf(stderr, "CM_LIVE_DHGR %s idx%d sand%d done\n", roomNm, roomIdx, sand);
		return 0;
	}

	// Parse the image-offset table exactly like Pics::ImageFile.
	const uint8_t *fb = pic->data;
	size_t flen = pic->datasize;
	uint16_t version = fb[0] | (fb[1] << 8);
	size_t base = (version == 0x1000) ? 4 : 0;
	uint16_t offsets[16];
	for (int i = 0; i < 16; i++) {
		size_t p = base + i * 2;
		offsets[i] = fb[p] | (fb[p+1] << 8);
		if (version == 0x1000) offsets[i] += 4;
	}
	if (index < 0 || index > 15) { fprintf(stderr, "index out of range\n"); return 1; }
	uint16_t start = offsets[index];
	const char *startEnv = getenv("PIXTEST_START");
	if (startEnv) start = (uint16_t)strtol(startEnv, nullptr, 0);
	if (start >= flen) { fprintf(stderr, "offset %u >= file size %zu\n", start, flen); return 1; }

	fprintf(stderr, "file=%s size=%zu version=%04x index=%d start=%u\n",
		wantName, flen, version, index, start);

	const char *imgPath = getenv("PIXTEST_IMG");
	if (imgPath) {
		FILE *imf = fopen(imgPath, "wb");
		if (imf) { fwrite(fb + start, 1, flen - start, imf); fclose(imf); }
	}

	// Reset to a white page (room background) like drawPicture() for a fresh room.
	// PIXTEST_BLACK=1 starts from black instead (the CM panel background), so
	// white-on-white sand stays visible.
	gmResetScreen(getenv("PIXTEST_BLACK") == nullptr);
	const char *otp = getenv("PIXTEST_OPTRACE");
	if (otp) { g_optrace = fopen(otp, "w"); g_gmOnOp = onOp; }
	g_gmWriteLog = logWrite;
	gmDrawImage(fb + start, flen - start);
	g_gmWriteLog = nullptr;
	g_gmOnOp = nullptr;
	if (g_optrace) fclose(g_optrace);

	// Hourglass overlay (CM): after rendering the panel logo (RG index 0), stamp
	// the sand pile for CM_HOURGLASS grains so it can be dumped/eyeballed.
	const char *hg = getenv("CM_HOURGLASS");
	if (hg) {
		int sand = (int)strtol(hg, nullptr, 0);
		// CM_HOURGLASS_FALL=N dumps the Nth grain-fall frame (a debugging aid for
		// the per-turn animation), faithfully mirroring pics.cpp: the freshly
		// drawn OG0 (clean hourglass frame, no grains) is captured as the panel,
		// then the pile is stamped on a clean overlay at the post-drop level, a
		// single-grain drop is armed, and the fall stepped to frame N. (N >=
		// kCMFallFrames is the final clear frame, which must leave the neck clean.)
		const char *fall = getenv("CM_HOURGLASS_FALL");
		if (fall) {
			gmCaptureCMPanel(24, 39);          // clean OG0 frame, no grains
			gmDrawCMHourglass(sand);           // first draw: displayed=sand, no arm
			gmOverlayCMPanel();                // restore clean, as a new turn does
			gmDrawCMHourglass(sand - 1);       // drop one grain -> arms the fall
			gmCMHourglassConsumeFallArmed();
			gmCMHourglassFallBegin();
			int want = (int)strtol(fall, nullptr, 0), y0, y1;
			for (int f = 0; f <= want && gmCMHourglassFallActive(); f++)
				gmCMHourglassFallStep(&y0, &y1);
		} else {
			gmDrawCMHourglass(sand);
		}
	}

	if (g_out != stdout) fclose(g_out);

	const char *pagePath = getenv("PIXTEST_PAGE");
	if (pagePath) {
		FILE *pf = fopen(pagePath, "wb");
		if (pf) { fwrite(gmPagePtr(), 1, 0x2000, pf); fclose(pf); }
	}
	fprintf(stderr, "wrote %ld screen writes\n", g_count);
	return 0;
}

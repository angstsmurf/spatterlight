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
		uint8_t *fx = readFile("test/talisman/t2.bin", &fxSz);
		if (!fx) fx = readFile("terps/comprehend/test/talisman/t2.bin", &fxSz);
		if (fx && gmInstallDrawingTables(fx, fxSz)) {
			fprintf(stderr, "loaded drawing tables from test/talisman/t2.bin\n");
		} else {
			fprintf(stderr, "warning: 'T2' not on this disk and no t2.bin fixture; drawing tables left zero\n");
		}
		free(fx);
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
	gmResetScreen(true);
	const char *otp = getenv("PIXTEST_OPTRACE");
	if (otp) { g_optrace = fopen(otp, "w"); g_gmOnOp = onOp; }
	g_gmWriteLog = logWrite;
	gmDrawImage(fb + start, flen - start);
	g_gmWriteLog = nullptr;
	g_gmOnOp = nullptr;
	if (g_optrace) fclose(g_optrace);

	if (g_out != stdout) fclose(g_out);

	const char *pagePath = getenv("PIXTEST_PAGE");
	if (pagePath) {
		FILE *pf = fopen(pagePath, "wb");
		if (pf) { fwrite(gmPagePtr(), 1, 0x2000, pf); fclose(pf); }
	}
	fprintf(stderr, "wrote %ld screen writes\n", g_count);
	return 0;
}

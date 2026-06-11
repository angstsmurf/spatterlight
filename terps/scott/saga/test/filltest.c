// Standalone byte-exact harness for the Saga/Scott Apple II and Atari 8-bit
// vector renderers (which include apple2_flood_fill / flood_fill). It mirrors
// -[SpatterlightTests testScottDraw]: load the per-game drawing tables from
// MI.dsk, then render each regression image straight from its pre-extracted
// .dat opcode stream and compare the full screen buffer(s) against the saved
// .result dumps. so_painting / so_console lean hard on the flood fills.
//
//   usage: filltest <supportpath>   (dir holding MI.dsk, apple2*.dat/.result …)
//
// It links only the vector-draw + disk-decode modules; symbols belonging to the
// interpreter core / bitmap drawing are stubbed below and are never reached on
// the room-image render path.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "glk.h"
#include "sagagraphics.h"
#include "scott_defines.h"

// Defined in the linked modules.
int TestApple2ImageWithName(const char *name, const char *supportpath);
int TestAtari8ImageWithName(const char *name, const char *supportpath);
int DrawApple2VectorImage(USImage *img);
uint8_t *ReadTestDataFromFile(const char *filename, const char *supportpath, size_t *size);
void InitDskImage(uint8_t *data, size_t datasize);
int LoadDrawingDataFromDisk(uint8_t *data, size_t datasize);
void FreeDiskImage(void);

// ---- stubs --------------------------------------------------------------------
// Globals owned by modules we don't link (bitmap drawing / interpreter core).
uint8_t *screenmem = NULL;       // common_sagadraw/apple2draw.c
uint8_t *descrambletable = NULL; // common_sagadraw/apple2draw.c
int vector_image_shown = -1;     // ai_uk/line_drawing.c
int gli_slowdraw = 0;            // glkimp (extern under SPATTERLIGHT)
const char *game_file = NULL;    // scott.c
USImage *USImages = NULL;        // sagagraphics.c (image list head)
int CurrentSys, ImageWidth, ImageHeight;
void *Game;

void Fatal(const char *x) {
	fprintf(stderr, "Fatal: %s\n", x);
	exit(1);
}

USImage *NewImage(void) {
	USImage *img = calloc(1, sizeof(USImage));
	img->index = -1;
	return img;
}

// Filesystem walk calls this per file; returning 0 ("not a packed Saga image")
// lets the load-address header in ExtractAtAddress gate the real extraction.
int IsSagaImage(const char *name) { (void)name; return 0; }

// Functions referenced by the linked objects but never reached on this path
// (full game detection / .woz conversion / non-room bitmap draw). C linkage
// resolves by name, so simplified signatures are fine.
static void unreached(const char *who) { Fatal(who); }
void *LoadBinaryDatabase(void) { unreached("LoadBinaryDatabase"); return 0; }
void *LookInDatabase(void) { unreached("LookInDatabase"); return 0; }
void *woz2nib(void) { unreached("woz2nib"); return 0; }
void DrawApple2ImageFromVideoMem(void) { unreached("DrawApple2ImageFromVideoMem"); }
void DrawSingleApple2ImageByte(void) { unreached("DrawSingleApple2ImageByte"); }
void DrawSomeHowarthVectorPixels(void) { unreached("DrawSomeHowarthVectorPixels"); }
void DrawingHowarthVector(void) { unreached("DrawingHowarthVector"); }
void PutDoublePixel(glsi32 x, glsi32 y, int32_t color) { (void)x;(void)y;(void)color; unreached("PutDoublePixel"); }
void RectFill(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color) { (void)x;(void)y;(void)w;(void)h;(void)color; unreached("RectFill"); }
void SetColor(int32_t index, glui32 color) { (void)index;(void)color; unreached("SetColor"); }
void glk_request_timer_events(glui32 millisecs) { (void)millisecs; }

// ---- ground-truth corpus driver ---------------------------------------------
// Walk groundtruth/<game>/*.dat (315 MAME-captured Apple II vector pictures),
// render each straight from its raw opcode stream, and byte-compare the resulting
// $2000 hi-res page against the matching golden <name>.page. Every .dat — room
// (R*) and object overlay (B*) alike — is rendered with usage IMG_ROOM, because
// the goldens were captured by the real interpreter's DRAW_OPCODE, which always
// clears the page to white first (see groundtruth/README.md).

#define A2_PAGE_SIZE 0x2000

static uint8_t *read_whole_file(const char *path, size_t *size) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0) { fclose(f); return NULL; }
	uint8_t *buf = malloc((size_t)n);
	if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
	fclose(f);
	if (buf && size) *size = (size_t)n;
	return buf;
}

static int cmp_str(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

// Render one .dat and compare against its .page. Returns 1 on a byte-exact match.
static int compare_one_image(const char *dir, const char *datname) {
	char base[64];
	snprintf(base, sizeof base, "%.*s", (int)(strlen(datname) - 4), datname); // strip ".dat"

	char datpath[1024], pagepath[1024];
	snprintf(datpath, sizeof datpath, "%s/%s", dir, datname);
	snprintf(pagepath, sizeof pagepath, "%s/%s.page", dir, base);

	size_t datsize = 0;
	uint8_t *datbuf = read_whole_file(datpath, &datsize);
	if (!datbuf || datsize < 5) {
		fprintf(stderr, "  %s: cannot read .dat\n", base);
		free(datbuf);
		return 0;
	}

	USImage *img = NewImage();
	img->imagedata = datbuf;
	img->datasize = datsize;
	img->usage = IMG_ROOM;
	DrawApple2VectorImage(img);
	free(datbuf);
	free(img);

	size_t pagesize = 0;
	uint8_t *page = read_whole_file(pagepath, &pagesize);
	if (!page || pagesize < A2_PAGE_SIZE) {
		fprintf(stderr, "  %s: cannot read .page golden\n", base);
		free(page);
		return 0;
	}

	int firstdiff = -1, ndiff = 0;
	uint8_t goldbyte = 0;
	int verbose = getenv("GTVERBOSE") != NULL;
	for (int i = 0; i < A2_PAGE_SIZE; i++) {
		if (page[i] != screenmem[i]) {
			if (firstdiff < 0) { firstdiff = i; goldbyte = page[i]; }
			ndiff++;
			if (verbose)
				fprintf(stderr, "    %s diff @0x%04x: golden 0x%02x got 0x%02x\n",
				        base, i, page[i], screenmem[i]);
		}
	}
	free(page);

	if (ndiff) {
		fprintf(stderr, "  FAIL %s: %d byte(s) differ, first at 0x%x (golden 0x%02x, got 0x%02x)\n",
		        base, ndiff, firstdiff, goldbyte, screenmem[firstdiff]);
		return 0;
	}
	return 1;
}

static int run_groundtruth_corpus(const char *root) {
	static const char *games[] = { "adventureland", "mission", "pirate", "strange" };
	int total = 0, passed = 0;

	for (int g = 0; g < (int)(sizeof games / sizeof games[0]); g++) {
		char dir[1024];
		snprintf(dir, sizeof dir, "%s/%s", root, games[g]);

		DIR *d = opendir(dir);
		if (!d) {
			fprintf(stderr, "warning: cannot open %s\n", dir);
			continue;
		}

		// Collect the .dat names so the run order is stable and readable.
		char **names = NULL;
		int count = 0, cap = 0;
		struct dirent *ent;
		while ((ent = readdir(d)) != NULL) {
			size_t len = strlen(ent->d_name);
			if (len > 4 && strcmp(ent->d_name + len - 4, ".dat") == 0) {
				if (count == cap) { cap = cap ? cap * 2 : 64; names = realloc(names, cap * sizeof *names); }
				names[count++] = strdup(ent->d_name);
			}
		}
		closedir(d);
		qsort(names, count, sizeof *names, cmp_str);

		int gpass = 0;
		for (int i = 0; i < count; i++) {
			gpass += compare_one_image(dir, names[i]);
			free(names[i]);
		}
		free(names);

		printf("%-14s : %3d/%3d pages match\n", games[g], gpass, count);
		passed += gpass;
		total += count;
	}

	printf("\nground truth: %d/%d pages byte-exact\n", passed, total);
	return passed == total;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <supportpath> [groundtruth_root]\n", argv[0]);
		return 2;
	}
	const char *support = argv[1];

	// Apple II rendering needs the per-game pattern/brush tables, which the real
	// pipeline extracts from the game disk before drawing any picture.
	size_t disksize;
	uint8_t *disk = ReadTestDataFromFile("MI.dsk", support, &disksize);
	if (disk) {
		InitDskImage(disk, disksize);
		LoadDrawingDataFromDisk(disk, disksize);
		FreeDiskImage();
		free(disk);
	} else {
		fprintf(stderr, "warning: MI.dsk not found; Apple II cases will fail\n");
	}

	struct { const char *label, *name; int (*fn)(const char *, const char *); } cases[] = {
		{ "apple2 mi_boom",     "mi_boom",     TestApple2ImageWithName },
		{ "apple2 so_console",  "so_console",  TestApple2ImageWithName },
		{ "apple2 so_painting", "so_painting", TestApple2ImageWithName },
		{ "atari8 mi_boom",     "mi_boom",     TestAtari8ImageWithName },
		{ "atari8 so_console",  "so_console",  TestAtari8ImageWithName },
		{ "atari8 so_painting", "so_painting", TestAtari8ImageWithName },
	};
	int n = (int)(sizeof(cases) / sizeof(cases[0]));

	int passed = 0;
	for (int i = 0; i < n; i++) {
		int ok = cases[i].fn(cases[i].name, support);
		printf("%-20s : %s\n", cases[i].label, ok ? "PASS" : "FAIL");
		passed += ok;
	}
	printf("\n%d/%d cases passed\n", passed, n);
	int fixtures_ok = (passed == n);

	// Optional second argument: run the full Apple II ground-truth corpus
	// (groundtruth/<game>/*.dat vs *.page). The drawing tables loaded above
	// from MI.dsk cover all four games.
	int corpus_ok = 1;
	if (argc >= 3) {
		printf("\n--- Apple II ground-truth corpus ---\n");
		corpus_ok = run_groundtruth_corpus(argv[2]);
	}

	return (fixtures_ok && corpus_ok) ? 0 : 1;
}

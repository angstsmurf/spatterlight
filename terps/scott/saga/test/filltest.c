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

#include "glk.h"
#include "sagagraphics.h"

// Defined in the linked modules.
int TestApple2ImageWithName(const char *name, const char *supportpath);
int TestAtari8ImageWithName(const char *name, const char *supportpath);
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

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <supportpath>\n", argv[0]);
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
	return passed == n ? 0 : 1;
}

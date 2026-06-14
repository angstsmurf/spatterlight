// Link-time stubs for the headless image-blob dumper (extract_images.c).
// The dumper runs only DetectGame's data-loading path; the C64 decrunch
// subsystem (C++ unp64) and all the Glk/glkimp rendering + UI live behind these
// stubs and are never reached while extracting image blobs. Real Glk is provided
// by cheapglk; only the glkimp-specific extras are stubbed here.

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "glk.h"
#include "detect_game.h"

// --- C64 detection (pulls in the C++ unp64 depacker) — not needed here --------
GameIDType DetectC64(uint8_t **sf, size_t *extent, const char *filename) {
    (void)sf; (void)extent; (void)filename;
    return UNKNOWN_GAME;
}

// --- glkimp-specific globals (defined by the glkimp lib in the real build) -----
// gli_slowdraw / gli_determinism are provided by cheapglk's cgmisc.o.
int gli_enable_graphics = 1;
int gli_flicker = 0;
int gli_sa_delays = 0;
int gli_sa_inventory = 0;
int gli_sa_palette = 0;
uint32_t gbgcol = 0;

// Config globals normally provided by cheapglk's main.o (which we exclude to
// keep our own main()).
int gli_screenwidth = 80, gli_screenheight = 200;
int gli_utf8output = 1, gli_utf8input = 1;
int gli_debugger = 0;
int gli_get_dataresource_info(int num, void **ptr, glui32 *len, int *isbinary) {
    (void)num; (void)ptr; (void)len; (void)isbinary; return 0;
}

// --- glkimp-specific helpers ---------------------------------------------------
GLK_ATTRIBUTE_NORETURN void Fatal(const char *x) {
    fprintf(stderr, "Fatal: %s\n", x ? x : "(null)");
    exit(1);
}
void win_testresult(int result) { (void)result; }
winid_t FindGlkWindowWithRock(glui32 rock) { (void)rock; return NULL; }
char *LineBreakText(const char *source, int columns, int *rows, int *length) {
    (void)source; (void)columns; if (rows) *rows = 0; if (length) *length = 0; return NULL;
}
glui32 erkyrath_random(void) { return 0; }
void set_erkyrath_random(glui32 seed) { (void)seed; }
int xstrncasecmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }

// --- Taylor renderer (separate interpreter; never called from the scott path) --
typedef void (*draw_obj_fn)(uint8_t, uint8_t);
void InitTaylor(uint8_t *data, uint8_t *end, uint8_t *objloc, int older, int rebel, draw_obj_fn obj_draw) {
    (void)data; (void)end; (void)objloc; (void)older; (void)rebel; (void)obj_draw;
}
int DrawTaylor(int loc, int current_location) { (void)loc; (void)current_location; return 0; }

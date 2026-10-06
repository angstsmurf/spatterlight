// Scene harness: runs the real ScottFree interpreter (glk_main) against a
// command script with a framebuffer-backed graphics window, and compares what
// the game has painted with a memory dump taken from real hardware (emulator)
// at the same point of the same game.
//
// Unlike the per-renderer tests (zxtest, c64test, ...), nothing is unity-
// included here: the whole picture path runs as shipped — DetectGame, the
// game-specific Look routines (Hulk item overlays, Robin of Sherwood forest
// composites, Gremlins animations, Seas of Blood's Taylor-style pictures), the
// Howarth vector renderer, palette selection/remapping, the image patch tables
// and the ZX loading screen — down to PutPixel/RectFill, whose
// glk_window_fill_rect calls land in the canvas below.
//
// The interpreter sources are built with -include scene_glk.h, which renames
// the Glk window entry points to the fake window layer shared with the image
// probes (common_imagetest/fake_glk_window.c) and the event loop to the
// scene_* functions defined here. Streams, files and unicode still come from
// cheapglk.
//
//   scenetest [-v] [-d dumpdir] <file.scene>
//     -v  print the game's text output (a transcript) to stdout
//     -d  write every checked frame to <dumpdir>/<golden>.png
//
// A .scene file is a command script. Each plain line answers one input request
// (a command for line input; its first character for a keypress, Return if the
// line is empty). Lines starting with '#' are comments, lines starting with '!'
// are directives:
//
//   !game <file>       the game image, relative to the scene file (required)
//   !slowdraw          run with the slow-draw setting on (before the game starts)
//   !tick [n]          deliver n timer events (default: until the game stops
//                      the timer) — animation frames, slow drawing
//   !terp <line>       input like a plain line, but skipped when the same script
//                      is replayed on the original (see below)
//   !picture <n>       make picture n the picture of every room (the next LOOK
//                      shows it): for pictures no script can walk to. The
//                      original gets a `!hwpoke` of its picture table instead.
//                      Also blanks the picture, as the original does before
//                      it draws a room: some pictures are only overlays
//   !check <golden> [min=N] [dx=N] [dy=N]
//                      compare the graphics window as it is when the game next
//                      waits for input. Every painted pixel must match unless
//                      min= gives the number of pixels that have to; dx/dy
//                      shift the golden (golden pixel x+dx,y+dy <-> canvas x,y).
//
// Timer events are also delivered whenever the game waits with no input request
// pending (timed pauses, lightning flashes).
//
// Seas of Blood is built with AUTOWIN (see the Makefile): its battles are won
// without a dice roll, and take no input but the <HIT ENTER> before and after.
//
// The same script drives the original game when the goldens are captured
// (zx_capture.lua, a MAME autoboot script, for the Spectrum; c64_capture.py,
// which drives VICE, for the C64): plain lines are typed there too and every
// `!check` dumps the screen. Lines for the original only — `!hw <keys>`,
// `!hwkey <keys>`, `!hwwait <n>`, `!hwnext`, `!hwpoke <addr>=<bytes>`,
// `!dump <file>` — are ignored here.
// An animated picture is dumped frame by frame there (`!hwnext`, `!dump`) and
// stepped through here with `!tick` and a `!check` of each dump.
//
// Goldens, by extension:
//   .scr   ZX Spectrum SCREEN$: 6912 bytes of display file from $4000
//   .c64   Commodore 64 hires bitmap: 8000 bytes of bitmap + 1000 bytes of
//          screen RAM (colour nibbles)
// Both are decoded to colour indices and looked up in the interpreter's own
// pal[], so a wrong palette or remap fails the test just like a wrong pixel.

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "glk.h"
#include "glkstart.h"

extern void gli_initialize_misc(void);
extern int gli_slowdraw, gli_determinism;
extern int ImageWidth, ImageHeight;
extern glui32 pal[16];
extern void glk_main(void);

#include "scott.h"
#include "fake_glk_window.h"

// ---- Script ------------------------------------------------------------------

static char **script;
static int script_len, script_pos;
static const char *scene_path;
static char scene_dir[1024];
static const char *dump_dir;
static int verbose;
static int checks_run, checks_failed;

static void scene_fail(const char *msg)
{
    fprintf(stderr, "%s:%d: %s\n", scene_path, script_pos, msg);
    checks_failed++;
}

static void finish(void)
{
    for (; script_pos < script_len; script_pos++)
        if (strncmp(script[script_pos], "!check", 6) == 0) {
            script_pos++;
            scene_fail("the game exited before this check");
            script_pos--;
        }
    if (checks_run == 0 && checks_failed == 0) {
        fprintf(stderr, "%s: no checks\n", scene_path);
        exit(2);
    }
    exit(checks_failed ? 1 : 0);
}

static void load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        exit(2);
    }
    char line[1024];
    int cap = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#')
            continue;
        if (script_len == cap)
            script = realloc(script, (cap = cap ? cap * 2 : 64) * sizeof *script);
        script[script_len++] = strdup(line);
    }
    fclose(f);

    const char *slash = strrchr(path, '/');
    size_t dirlen = slash ? (size_t)(slash - path + 1) : 0;
    if (dirlen >= sizeof scene_dir)
        dirlen = sizeof scene_dir - 1;
    memcpy(scene_dir, path, dirlen);
    scene_dir[dirlen] = 0;
}

// ---- Fake windows (common_imagetest/fake_glk_window.c) ------------------------

#define canvas fakeglk_canvas
#define CANVAS_W FAKEGLK_CANVAS_W
#define CANVAS_H FAKEGLK_CANVAS_H
#define UNSET FAKEGLK_UNSET
#define graphics fakeglk_graphics

static long ticks_owed;

const char *fakeglk_name = "scenetest";

/* The graphics window is exactly one picture large, so the interpreter picks
   a pixel size of 1 and no margins: canvas coordinates are picture pixels. */
void fakeglk_graphics_size(int *w, int *h)
{
    *w = ImageWidth < 256 ? 256 : ImageWidth;
    *h = ImageHeight;
}

winid_t FindGlkWindowWithRock(glui32 rock)
{
    for (int i = 0; i < FAKEGLK_MAX_WINDOWS; i++)
        if (fakeglk_windows[i] && fakeglk_windows[i]->rock == rock)
            return (winid_t)fakeglk_windows[i];
    return NULL;
}

void scene_exit(void)
{
    finish();
}

// ---- Goldens -------------------------------------------------------------------

#define TRUTH_W 320
#define TRUTH_H 200

/* Colour indices (into the interpreter's pal[]) of the hardware picture. */
static uint8_t truth[TRUTH_H][TRUTH_W];
static int truth_w, truth_h;

static int load_truth(const char *path)
{
    static uint8_t buf[9001];
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 0;
    }
    size_t size = fread(buf, 1, sizeof buf, f);
    fclose(f);

    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".scr") == 0 && size == 6912) {
        truth_w = 256;
        truth_h = 192;
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++) {
                /* Display file layout: the three bit fields of y are shuffled. */
                int addr = ((y & 0xc0) << 5) | ((y & 0x07) << 8) | ((y & 0x38) << 2) | (x >> 3);
                uint8_t attr = buf[6144 + (y >> 3) * 32 + (x >> 3)];
                int set = (buf[addr] >> (7 - (x & 7))) & 1;
                int bright = (attr & 0x40) ? 8 : 0;
                truth[y][x] = (set ? (attr & 7) : ((attr >> 3) & 7)) + bright;
            }
        return 1;
    }
    if (ext && strcmp(ext, ".c64") == 0 && size == 9000) {
        truth_w = 320;
        truth_h = 200;
        for (int y = 0; y < 200; y++)
            for (int x = 0; x < 320; x++) {
                int cell = (y >> 3) * 40 + (x >> 3);
                int set = (buf[cell * 8 + (y & 7)] >> (7 - (x & 7))) & 1;
                uint8_t colours = buf[8000 + cell];
                truth[y][x] = set ? (colours >> 4) : (colours & 15);
            }
        return 1;
    }
    fprintf(stderr, "%s: not a .scr (6912 bytes) or .c64 (9000 bytes) golden\n", path);
    return 0;
}

/* Write the graphics window as a PNG; unpainted pixels come out mid-grey. */
static void dump_png(const char *name, int w, int h)
{
    char path[2048];
    const char *base = strrchr(name, '/');
    snprintf(path, sizeof path, "%s/%s.png", dump_dir, base ? base + 1 : name);
    fakeglk_write_png(path, w, h);
}

static void check(char *args)
{
    char *name = strtok(args, " \t");
    long min = -1;
    int dx = 0, dy = 0;
    if (!name) {
        scene_fail("!check needs a golden file");
        return;
    }
    for (char *opt; (opt = strtok(NULL, " \t")) != NULL;) {
        if (strncmp(opt, "min=", 4) == 0)
            min = atol(opt + 4);
        else if (strncmp(opt, "dx=", 3) == 0)
            dx = atoi(opt + 3);
        else if (strncmp(opt, "dy=", 3) == 0)
            dy = atoi(opt + 3);
        else {
            scene_fail("unknown !check option");
            return;
        }
    }

    checks_run++;
    printf("%-44s ", name);
    if (!graphics) {
        printf("no graphics window  FAIL\n");
        checks_failed++;
        return;
    }
    int w, h;
    fakeglk_graphics_window_size(&w, &h);
    if (dump_dir)
        dump_png(name, w, h);

    char path[2048];
    snprintf(path, sizeof path, "%s%s", scene_dir, name);
    if (!load_truth(path)) {
        printf("FAIL\n");
        checks_failed++;
        return;
    }

    long match = 0, total = 0;
    int firstx = -1, firsty = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int tx = x + dx, ty = y + dy;
            if (canvas[y][x] == UNSET || tx < 0 || ty < 0 || tx >= truth_w || ty >= truth_h)
                continue;
            total++;
            if (canvas[y][x] == pal[truth[ty][tx]])
                match++;
            else if (firstx < 0) {
                firstx = x;
                firsty = y;
            }
        }
    int ok = total > 0 && (min >= 0 ? match >= min : match == total);
    printf("%6ld/%6ld px match (%.2f%%)%s\n", match, total, 100.0 * match / (total ? total : 1),
        ok ? "  PASS" : "  FAIL");
    if (!ok) {
        checks_failed++;
        if (min >= 0)
            fprintf(stderr, "  expected at least %ld matching pixels\n", min);
        if (firstx >= 0)
            fprintf(stderr, "  first diff at (%d,%d): got #%06x, hardware #%06x\n", firstx, firsty,
                canvas[firsty][firstx], pal[truth[firsty + dy][firstx + dx]]);
    }
}

// ---- Events --------------------------------------------------------------------

void scene_select(event_t *ev)
{
    /* A game spinning on timer events forever is a broken scene, not a hang. */
    static long idle_ticks;

    memset(ev, 0, sizeof *ev);
    for (;;) {
        fakeglk_window *win = fakeglk_requesting_window();
        if (fakeglk_timer_ms && (ticks_owed > 0 || !win)) {
            if (ticks_owed > 0)
                ticks_owed--;
            else if (++idle_ticks > 1000000) {
                scene_fail("the game keeps waiting for timer events");
                finish();
            }
            ev->type = evtype_Timer;
            return;
        }
        ticks_owed = 0;
        idle_ticks = 0;
        if (!win) {
            scene_fail("the game waits for an event but has requested none");
            finish();
        }

        if (script_pos >= script_len)
            finish();
        char *line = script[script_pos++];
        if (strncmp(line, "!terp", 5) == 0)
            line += line[5] ? 6 : 5;
        if (strncmp(line, "!check", 6) == 0) {
            char *args = strdup(line + 6);
            check(args);
            free(args);
        } else if (strncmp(line, "!picture", 8) == 0) {
            for (int i = 0; i <= GameHeader.NumRooms; i++)
                Rooms[i].Image = atoi(line + 8);
            fakeglk_clear_canvas();
        } else if (strncmp(line, "!tick", 5) == 0) {
            ticks_owed = atol(line + 5);
            if (ticks_owed <= 0)
                ticks_owed = 1000000;
        } else if (strncmp(line, "!hw", 3) == 0 || strncmp(line, "!dump", 5) == 0) {
            /* For zx_capture.lua only. */
        } else if (line[0] == '!') {
            scene_fail("unknown directive");
        } else if (win->line_request) {
            glui32 len = 0;
            while (line[len] && len < win->linemax) {
                win->linebuf[len] = (unsigned char)line[len];
                len++;
            }
            if (verbose)
                printf("%s\n", line);
            win->line_request = 0;
            ev->type = evtype_LineInput;
            ev->win = (winid_t)win;
            ev->val1 = len;
            return;
        } else {
            win->char_request = 0;
            ev->type = evtype_CharInput;
            ev->win = (winid_t)win;
            ev->val1 = line[0] ? (unsigned char)line[0] : keycode_Return;
            return;
        }
    }
}

// ---- Main ----------------------------------------------------------------------

int main(int argc, char **argv)
{
    int arg = 1;
    for (; arg < argc && argv[arg][0] == '-'; arg++) {
        if (strcmp(argv[arg], "-v") == 0)
            verbose = 1;
        else if (strcmp(argv[arg], "-d") == 0 && arg + 1 < argc)
            dump_dir = argv[++arg];
        else
            break;
    }
    if (arg != argc - 1) {
        fprintf(stderr, "usage: %s [-v] [-d dumpdir] <file.scene>\n", argv[0]);
        return 2;
    }
    scene_path = argv[arg];
    load_script(scene_path);

    /* Directives that take effect before the game starts. */
    static char game_path[2048];
    int kept = 0;
    for (int i = 0; i < script_len; i++) {
        if (strncmp(script[i], "!game ", 6) == 0)
            snprintf(game_path, sizeof game_path, "%s%s", scene_dir, script[i] + 6);
        else if (strcmp(script[i], "!slowdraw") == 0)
            gli_slowdraw = 1;
        else
            script[kept++] = script[i];
    }
    script_len = kept;
    if (!game_path[0]) {
        fprintf(stderr, "%s: no !game line\n", scene_path);
        return 2;
    }

    gli_initialize_misc();
    gli_determinism = 1;
    fakeglk_null_stream = glk_stream_open_memory(NULL, 0, filemode_Write, 0);
    fakeglk_text_stream = verbose ? glk_window_get_stream(glk_window_open(0, 0, 0, wintype_TextBuffer, 0))
                                  : fakeglk_null_stream;

    char *game_argv[] = { argv[0], game_path, NULL };
    glkunix_startup_t startdata = { 2, game_argv };
    if (!glkunix_startup_code(&startdata))
        return 2;
    glk_main();
    finish();
    return 0;
}

/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/* Scene harness: runs the real ScottFree interpreter (glk_main) against a
   command script with a framebuffer-backed graphics window, and compares what
   the game has painted with a memory dump taken from real hardware (emulator)
   at the same point of the same game.

   Unlike the per-renderer tests (zxtest, c64test, ...), nothing is unity-
   included here: the whole picture path runs as shipped — DetectGame, the
   game-specific Look routines (Hulk item overlays, Robin of Sherwood forest
   composites, Gremlins animations, Seas of Blood's Taylor-style pictures), the
   Howarth vector renderer, palette selection/remapping, the image patch tables
   and the ZX loading screen — down to PutPixel/RectFill, whose
   glk_window_fill_rect calls land in the canvas below.

   The interpreter sources are built with -include scene_glk.h, which renames
   the Glk window entry points to the fake window layer shared with the image
   probes (common_imagetest/fake_glk_window.c) and the event loop to the
   scene_* functions defined here. Streams, files and unicode still come from
   cheapglk.

     scenetest [-v|-vv] [-c] [-t] [-d dumpdir] <file.scene>
       -v  print the game's text output (a transcript) to stdout
       -vv the room descriptions too (as a transcript has them), for writing a
           scene
       -c  print at every check how many rectangles were filled since the one
           before: the cost of drawing the picture
       -d  write every checked frame to <dumpdir>/<golden>.png
       -t  print the game's rooms and items, with their pictures, at the first
           input request, and stop: what a scene generator needs to know

   The .scene format and the goldens are described in README.md. */

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
extern void glk_main(void);

#include "aiukpalette.h"
#include "scott.h"
#include "saga.h"
#include "c64_small.h"
#include "c64a8draw.h"
#include "sagagraphics.h"
#include "fake_glk_window.h"
#include "test_util.h"

/* ---- Script ------------------------------------------------------------------ */

static char **script;
static int script_len, script_pos;
static const char *scene_path;
static char scene_dir[1024];
static const char *dump_dir;
static int verbose;
static int count_rects;
static int print_tables;
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

/* ---- Fake windows (common_imagetest/fake_glk_window.c) ------------------------ */

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

/* ---- Goldens ------------------------------------------------------------------- */

#define TRUTH_W 320
#define TRUTH_H 200
#define TRUTH_UNSET 0xff /* a C64C2 pixel outside what was captured */

/* Colour indices (into truth_pal) of the hardware picture. */
static uint8_t truth[TRUTH_H][TRUTH_W];
static int truth_w, truth_h;
static const glui32 *truth_pal;

static int load_truth(const char *path)
{
    static uint8_t buf[TRUTH_W * TRUTH_H + 1024];
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 0;
    }
    size_t size = fread(buf, 1, sizeof buf, f);
    fclose(f);

    truth_pal = pal;
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".vic") == 0 && size == TRUTH_W * TRUTH_H) {
        truth_w = TRUTH_W;
        truth_h = TRUTH_H;
        truth_pal = c64colors;
        for (int y = 0; y < TRUTH_H; y++)
            for (int x = 0; x < TRUTH_W; x++)
                truth[y][x] = buf[y * TRUTH_W + x] & 15;
        return 1;
    }
    if (ext && strcmp(ext, ".scr") == 0 && size == 6912) {
        truth_w = 256;
        truth_h = 192;
        for (int y = 0; y < 192; y++)
            for (int x = 0; x < 256; x++)
                truth[y][x] = zx_scr_colour(buf, x, y);
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
    /* The goldens of dostest and c64a8test, which are hardware screens too. */
    int32_t header[3];
    if (ext && strcmp(ext, ".cga") == 0 && size >= 13 && memcmp(buf, "CGA00", 5) == 0) {
        static const glui32 cga_pal[4] = { 0x000000, 0x00ffff, 0xff00ff, 0xffffff };
        memcpy(header, buf + 5, 8);
        if (header[0] == TRUTH_W && header[1] == TRUTH_H && size == 13 + (size_t)TRUTH_W * TRUTH_H) {
            truth_w = TRUTH_W;
            truth_h = TRUTH_H;
            truth_pal = cga_pal;
            memcpy(truth, buf + 13, TRUTH_W * TRUTH_H);
            return 1;
        }
    }
    if (size >= 17 && memcmp(buf, "C64C2", 5) == 0) {
        static glui32 own_pal[255];
        memcpy(header, buf + 5, 12);
        size_t data = 17 + (size_t)header[2] * 3;
        if (header[0] == TRUTH_W && header[1] == TRUTH_H && header[2] < 255
            && size == data + (size_t)TRUTH_W * TRUTH_H) {
            for (int i = 0; i < header[2]; i++)
                own_pal[i] = buf[17 + i * 3] << 16 | buf[18 + i * 3] << 8 | buf[19 + i * 3];
            truth_w = TRUTH_W;
            truth_h = TRUTH_H;
            truth_pal = own_pal;
            memcpy(truth, buf + data, TRUTH_W * TRUTH_H);
            return 1;
        }
    }
    fprintf(stderr, "%s: not a .scr (6912 bytes), .c64 (9000 bytes), .vic (64000 bytes), .cga or C64C2 golden\n", path);
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
    if (count_rects) {
        printf("%-44s %6ld rectangles\n", name, fakeglk_fill_rect_count);
        fakeglk_fill_rect_count = 0;
    }
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
            if (canvas[y][x] == UNSET || tx < 0 || ty < 0 || tx >= truth_w || ty >= truth_h
                || truth[ty][tx] == TRUTH_UNSET)
                continue;
            total++;
            if (canvas[y][x] == truth_pal[truth[ty][tx]])
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
                canvas[firsty][firstx], truth_pal[truth[firsty + dy][firstx + dx]]);
    }
}

/* ---- Events -------------------------------------------------------------------- */

static void print_game_tables(void)
{
    printf("start %d lights %d\n", MyLoc, GameHeader.LightTime);
    for (int i = 0; i <= GameHeader.NumRooms; i++)
        printf("room %d image %d exits %d %d %d %d %d %d \"%s\"\n", i, Rooms[i].Image,
               Rooms[i].Exits[0], Rooms[i].Exits[1], Rooms[i].Exits[2],
               Rooms[i].Exits[3], Rooms[i].Exits[4], Rooms[i].Exits[5], Rooms[i].Text ? Rooms[i].Text : "");
    for (int i = 0; i <= GameHeader.NumItems; i++)
        printf("item %d image %d flag %d loc %d \"%s\"\n", i, Items[i].Image, Items[i].Flag,
            Items[i].InitialLoc, Items[i].Text ? Items[i].Text : "");
    for (int i = 0; i <= GameHeader.NumActions; i++) {
        printf("action %d verb %d noun %d if", i, Actions[i].Vocab / 150, Actions[i].Vocab % 150);
        for (int c = 0; c < 5; c++)
            printf(" %d:%d", Actions[i].Condition[c] % 20, Actions[i].Condition[c] / 20);
        printf(" do %d %d %d %d\n", Actions[i].Opcode[0] / 150, Actions[i].Opcode[0] % 150,
            Actions[i].Opcode[1] / 150, Actions[i].Opcode[1] % 150);
    }
    for (int i = 0; i <= GameHeader.NumMessages; i++)
        printf("message %d \"%s\"\n", i, Messages[i]);
    for (int i = 0; i <= GameHeader.NumWords; i++)
        printf("word %d \"%s\" \"%s\"\n", i, Verbs[i], Nouns[i]);
    exit(0);
}

void scene_select(event_t *ev)
{
    /* A game spinning on timer events forever is a broken scene, not a hang. */
    static long idle_ticks;

    memset(ev, 0, sizeof *ev);
    for (;;) {
        fakeglk_window *win = fakeglk_requesting_window();
        if (print_tables && win)
            print_game_tables();
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
            fakeglk_clear_canvas();
            if (USImages) {
                /* A US game draws its pictures by number, not by room:
                   show this one now, as a LOOK of the room whose entry in
                   the original's picture table was pointed at it does. */
                DrawUSRoom(atoi(line + 8));
                DrawImageOrVector();
            } else {
                for (int i = 0; i <= GameHeader.NumRooms; i++)
                    Rooms[i].Image = atoi(line + 8);
            }
        } else if (strncmp(line, "!room", 5) == 0) {
            MyLoc = atoi(line + 5);
        } else if (strncmp(line, "!counter", 8) == 0) {
            int counter, value;
            if (sscanf(line + 8, "%d %d", &counter, &value) != 2 || counter < 0 || counter >= 16)
                scene_fail("!counter needs a counter and a value");
            else
                Counters[counter] = value;
        } else if (strncmp(line, "!flag", 5) == 0) {
            BitFlags |= 1L << atoi(line + 5);
        } else if (strncmp(line, "!item", 5) == 0) {
            int item, room;
            if (sscanf(line + 5, "%d %d", &item, &room) != 2 || item < 0 || item > GameHeader.NumItems)
                scene_fail("!item needs an item and a room");
            else
                Items[item].Location = room;
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

/* ---- Main ---------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    int arg = 1;
    for (; arg < argc && argv[arg][0] == '-'; arg++) {
        if (strcmp(argv[arg], "-v") == 0)
            verbose = 1;
        else if (strcmp(argv[arg], "-vv") == 0)
            verbose = 2;
        else if (strcmp(argv[arg], "-c") == 0)
            count_rects = 1;
        else if (strcmp(argv[arg], "-t") == 0)
            print_tables = 1;
        else if (strcmp(argv[arg], "-d") == 0 && arg + 1 < argc)
            dump_dir = argv[++arg];
        else
            break;
    }
    if (arg != argc - 1) {
        fprintf(stderr, "usage: %s [-v|-vv] [-c] [-t] [-d dumpdir] <file.scene>\n", argv[0]);
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
    if (verbose > 1)
        Transcript = fakeglk_text_stream; /* the game copies the room descriptions here */

    char *game_argv[] = { argv[0], game_path, NULL };
    glkunix_startup_t startdata = { 2, game_argv };
    if (!glkunix_startup_code(&startdata))
        return 2;
    glk_main();
    finish();
    return 0;
}

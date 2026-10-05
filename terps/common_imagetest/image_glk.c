// Fake Glk window layer for the image probes of the Plus and TaylorMade
// interpreters (plus/test/image_probe.c, taylor/test/image_probe.c).
//
// The probe runs the real interpreter (glk_main) on one game file. The
// interpreter sources are built with -include image_glk.h, which renames the
// Glk window and event entry points to the image_* functions below; streams and
// files still come from cheapglk. So the game is detected, loaded and started
// exactly as shipped, the title picture is painted into the canvas below, and
// when the game first waits for a command the interpreter's own probe takes
// over (ImageProbeDump) and draws every picture through the interpreter's
// drawing code.
//
// What comes out is a fingerprint: one line per picture with the number of
// painted pixels, their bounding box and a CRC32 of the colours. It holds
// nothing of the game, so the goldens can be committed although the games
// cannot.
//
//   <probe> [-d dumpdir] [-s script] <game file> [keys]
//     -d    also write every picture to <dumpdir>/<label>.png
//     -s    play the game instead (see below)
//     keys  answers to the key presses the game asks for before its first
//           prompt, one character each (a menu choice on a compilation tape,
//           Y or N to a question). Return is pressed once they run out. A key
//           asked for on the graphics window (a title picture) always gets
//           Return and uses up none.
//
// With -s the probe plays the game instead of taking the fingerprint: every
// prompt is answered with the next line of the script, and what the game
// prints in its text buffer window goes to stdout, each command followed by a
// "[canvas xxxxxxxx]" line with the CRC32 of the graphics window as it was
// when the command was typed. It ends when the script does. A key press the
// game asks for once the keys have run out gets the next script line if that
// is a lone Y or N (the answer to a question), and Return otherwise. The
// output is the game's text, so it is for comparing two builds, not for
// committing.
//
// Same approach as scott/saga/test/scenetest.c, which compares single scenes
// with hardware screen dumps; this one has no script and no hardware goldens,
// it pins down what every picture of a game looks like today.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "glk.h"
#include "glkstart.h"
#include "cheapglk.h"

#include "image_probe.h"

extern void gli_initialize_misc(void);
extern int gli_determinism;
extern void glk_main(void);

/* Spatterlight front-end settings (glkimp.h). Graphics on, no delays, and the
   "nothing forced" value for the rest. */
int gli_enable_graphics = 1;
int gli_sa_delays = 0;
int gli_sa_inventory = 0;
int gli_sa_palette = 0;
uint32_t gfgcol = 0x000000;
uint32_t gbgcol = 0xffffff;

/* What CheapGlk's main.c would define; the probe has its own main(). */
int gli_screenwidth = 80;
int gli_screenheight = 24;
int gli_utf8output = 0;
int gli_utf8input = 0;
int gli_debugger = 0;

int gli_get_dataresource_info(int num, void **ptr, glui32 *len, int *isbinary)
{
    (void)num; (void)ptr; (void)len; (void)isbinary;
    return 0;
}

void win_beep_zx(int duration, int pitch)
{
    (void)duration; (void)pitch;
}

// ---- Fake windows --------------------------------------------------------------

#define CANVAS_W 640
#define CANVAS_H 400
#define UNSET 0xffffffffu

typedef struct {
    glui32 type, rock, size;
    strid_t str;
    int char_request, line_request;
} image_window;

#define MAX_WINDOWS 16
static image_window *windows[MAX_WINDOWS];
static image_window pair_window;
static image_window *graphics;
static uint32_t canvas[CANVAS_H][CANVAS_W];
static strid_t null_stream;
static glui32 timer_ms;
static const char *dump_dir;
static const char *keys = "";
static int dumped;

/* Play mode (-s). */
static FILE *script;
static strid_t stdout_stream;
static char *line_buf;
static glui32 line_maxlen;

void ImageProbeClear(void)
{
    for (int y = 0; y < CANVAS_H; y++)
        for (int x = 0; x < CANVAS_W; x++)
            canvas[y][x] = UNSET;
}

winid_t image_window_open(winid_t split, glui32 method, glui32 size, glui32 wintype, glui32 rock)
{
    (void)split;
    image_window *win = calloc(1, sizeof *win);
    win->type = wintype;
    win->rock = rock;
    win->size = (method & winmethod_Fixed) ? size : 24;
    win->str = (script && wintype == wintype_TextBuffer) ? stdout_stream : null_stream;
    if (wintype == wintype_Graphics) {
        graphics = win;
        ImageProbeClear();
    }
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i]) {
            windows[i] = win;
            return (winid_t)win;
        }
    fprintf(stderr, "image probe: too many windows\n");
    exit(2);
}

void image_window_close(winid_t w, stream_result_t *result)
{
    image_window *win = (image_window *)w;
    if (result)
        result->readcount = result->writecount = 0;
    if (!win)
        return;
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i] == win)
            windows[i] = NULL;
    if (win == graphics)
        graphics = NULL;
    free(win);
}

winid_t image_window_iterate(winid_t w, glui32 *rock)
{
    int i = 0;
    if (w)
        for (i = 1; i <= MAX_WINDOWS; i++)
            if (windows[i - 1] == (image_window *)w)
                break;
    for (; i < MAX_WINDOWS; i++)
        if (windows[i]) {
            if (rock)
                *rock = windows[i]->rock;
            return (winid_t)windows[i];
        }
    return NULL;
}

void image_window_clear(winid_t w)
{
    if (w && (image_window *)w == graphics)
        ImageProbeClear();
}

void image_window_get_size(winid_t w, glui32 *width, glui32 *height)
{
    image_window *win = (image_window *)w;
    int gw = 80, gh = win ? (int)win->size : 0;
    if (win && win->type == wintype_Graphics) {
        ImageProbeGraphicsSize(&gw, &gh);
        if (gw > CANVAS_W)
            gw = CANVAS_W;
        if (gh > CANVAS_H)
            gh = CANVAS_H;
    }
    if (width)
        *width = gw;
    if (height)
        *height = gh;
}

winid_t image_window_get_parent(winid_t w)
{
    return w ? (winid_t)&pair_window : NULL;
}

void image_window_set_arrangement(winid_t w, glui32 method, glui32 size, winid_t key)
{
    (void)w;
    if (key && (method & winmethod_Fixed))
        ((image_window *)key)->size = size;
}

strid_t image_window_get_stream(winid_t w)
{
    return w ? ((image_window *)w)->str : NULL;
}

void image_window_set_echo_stream(winid_t w, strid_t str)
{
    (void)w; (void)str;
}

void image_set_window(winid_t w)
{
    glk_stream_set_current(w ? ((image_window *)w)->str : NULL);
}

void image_window_move_cursor(winid_t w, glui32 x, glui32 y)
{
    (void)w; (void)x; (void)y;
}

void image_window_set_background_color(winid_t w, glui32 color)
{
    (void)w; (void)color;
}

void image_window_fill_rect(winid_t w, glui32 color, glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    if (!w || (image_window *)w != graphics)
        return;
    for (glsi32 y = top; y < top + (glsi32)height; y++)
        for (glsi32 x = left; x < left + (glsi32)width; x++)
            if (x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H)
                canvas[y][x] = color & 0xffffff;
}

glui32 image_style_measure(winid_t w, glui32 style, glui32 hint, glui32 *result)
{
    (void)w; (void)style; (void)hint; (void)result;
    return 0;
}

glui32 image_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 arrlen)
{
    switch (sel) {
    case gestalt_Graphics:
    case gestalt_DrawImage:
    case gestalt_GraphicsCharInput:
    case gestalt_Timer:
        return 1;
    }
    return glk_gestalt_ext(sel, val, arr, arrlen);
}

glui32 image_gestalt(glui32 sel, glui32 val)
{
    return image_gestalt_ext(sel, val, NULL, 0);
}

void image_exit(void)
{
    fflush(stdout);
    if (!dumped && !script) {
        fprintf(stderr, "image probe: the game exited before its first prompt\n");
        exit(1);
    }
    exit(0);
}

// ---- Fingerprint ---------------------------------------------------------------

uint32_t ImageProbeCRC(const void *data, size_t length)
{
    return (uint32_t)crc32(0, data, (uInt)length);
}

static void png_chunk(FILE *f, const char *tag, const uint8_t *data, uint32_t len)
{
    uint8_t head[8] = { len >> 24, len >> 16, len >> 8, len, tag[0], tag[1], tag[2], tag[3] };
    uint32_t crc = crc32(crc32(0, head + 4, 4), data, len);
    uint8_t tail[4] = { crc >> 24, crc >> 16, crc >> 8, crc };
    fwrite(head, 1, 8, f);
    fwrite(data, 1, len, f);
    fwrite(tail, 1, 4, f);
}

/* Write the graphics window as a PNG; unpainted pixels come out mid-grey. */
static void dump_png(const char *name)
{
    int w = 256, h = 96;
    ImageProbeGraphicsSize(&w, &h);
    for (int y = 0; y < CANVAS_H; y++)
        for (int x = 0; x < CANVAS_W; x++)
            if (canvas[y][x] != UNSET) {
                if (x >= w)
                    w = x + 1;
                if (y >= h)
                    h = y + 1;
            }

    char path[2048];
    int n = snprintf(path, sizeof path, "%s/", dump_dir);
    for (const char *p = name; *p && n < (int)sizeof path - 5; p++)
        path[n++] = (*p == ' ' || *p == '/') ? '_' : *p;
    strcpy(path + n, ".png");
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return;
    }
    size_t rawlen = (size_t)h * (w * 3 + 1);
    uint8_t *raw = malloc(rawlen), *p = raw;
    for (int y = 0; y < h; y++) {
        *p++ = 0;
        for (int x = 0; x < w; x++) {
            uint32_t c = canvas[y][x] == UNSET ? 0x808080 : canvas[y][x];
            *p++ = c >> 16;
            *p++ = c >> 8;
            *p++ = c;
        }
    }
    uLongf zlen = compressBound(rawlen);
    uint8_t *z = malloc(zlen);
    compress(z, &zlen, raw, rawlen);
    uint8_t ihdr[13] = { w >> 24, w >> 16, w >> 8, w, h >> 24, h >> 16, h >> 8, h, 8, 2, 0, 0, 0 };
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, (uint32_t)zlen);
    png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
}

void ImageProbeReport(const char *name, const char *detail)
{
    char label[256];
    snprintf(label, sizeof label, detail ? "%-8s %s" : "%s", name, detail);

    int x0 = CANVAS_W, y0 = CANVAS_H, x1 = -1, y1 = -1;
    long painted = 0;
    for (int y = 0; y < CANVAS_H; y++)
        for (int x = 0; x < CANVAS_W; x++)
            if (canvas[y][x] != UNSET) {
                painted++;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    if (!painted) {
        printf("%-24s nothing painted\n", label);
        return;
    }

    /* Three bytes per pixel of the bounding box; 0xff 0xff 0xff 0xff for a
       pixel inside it that was left unpainted. */
    uint32_t crc = crc32(0, NULL, 0);
    for (int y = y0; y <= y1; y++) {
        uint8_t row[CANVAS_W * 4];
        int n = 0;
        for (int x = x0; x <= x1; x++) {
            uint32_t c = canvas[y][x];
            if (c == UNSET)
                row[n++] = 0xff;
            row[n++] = c >> 16;
            row[n++] = c >> 8;
            row[n++] = c;
        }
        crc = crc32(crc, row, n);
    }
    printf("%-24s %6ld px  %3d,%-3d %3dx%-3d  %08x\n", label, painted, x0, y0,
        x1 - x0 + 1, y1 - y0 + 1, crc);
    if (dump_dir)
        dump_png(name);
}

void ImageProbeNote(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

static void start_fingerprint(void)
{
    static int started;
    if (!started)
        printf("\n== pictures\n");
    started = 1;
}

// ---- Events --------------------------------------------------------------------

void image_request_char_event(winid_t w)
{
    if (w)
        ((image_window *)w)->char_request = 1;
}

void image_cancel_char_event(winid_t w)
{
    if (w)
        ((image_window *)w)->char_request = 0;
}

void image_request_line_event(winid_t w, char *buf, glui32 maxlen, glui32 initlen)
{
    (void)initlen;
    line_buf = buf;
    line_maxlen = maxlen;
    if (w)
        ((image_window *)w)->line_request = 1;
}

void image_request_timer_events(glui32 millisecs)
{
    timer_ms = millisecs;
}

static image_window *requesting_window(void)
{
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i] && (windows[i]->line_request || windows[i]->char_request))
            return windows[i];
    return NULL;
}

/* Play mode: the next line of the script, without its line end. NULL at the
   end of the script. */
static char *next_script_line(void)
{
    static char line[512];
    if (!fgets(line, sizeof line, script))
        return NULL;
    line[strcspn(line, "\r\n")] = 0;
    return line;
}

/* Play mode: answer a prompt with the next line of the script. */
static void type_script_line(image_window *win, event_t *ev)
{
    char *line = next_script_line();
    if (!line) {
        printf("\n[end of script]\n");
        image_exit();
    }
    size_t length = strlen(line);
    if (length > line_maxlen)
        length = line_maxlen;
    memcpy(line_buf, line, length);
    printf("%.*s\n[canvas %08x]\n", (int)length, line, ImageProbeCRC(canvas, sizeof canvas));

    win->line_request = 0;
    ev->type = evtype_LineInput;
    ev->win = (winid_t)win;
    ev->val1 = (glui32)length;
}

/* Play mode: the key to press once the keys have run out. A lone Y or N next
   in the script is the answer to a question; anything else stays where it is
   and the key is Return. */
static glui32 script_key(void)
{
    long pos = ftell(script);
    char *line = next_script_line();
    if (line && (line[0] == 'Y' || line[0] == 'N') && line[1] == 0)
        return (unsigned char)line[0];
    fseek(script, pos, SEEK_SET);
    return keycode_Return;
}

void image_select(event_t *ev)
{
    static long idle_ticks, key_presses;

    memset(ev, 0, sizeof *ev);
    image_window *win = requesting_window();
    if (!win) {
        /* A timed pause or an animation: let the time pass. */
        if (!timer_ms || ++idle_ticks > 1000000) {
            fprintf(stderr, timer_ms ? "image probe: the game keeps waiting for timer events\n"
                                     : "image probe: the game waits for an event but has requested none\n");
            exit(1);
        }
        ev->type = evtype_Timer;
        return;
    }
    idle_ticks = 0;

    if (win->line_request && script) {
        type_script_line(win, ev);
        return;
    }

    if (win->line_request) {
        /* The first prompt: the game is loaded and started. */
        start_fingerprint();
        ImageProbeDump();
        dumped = 1;
        image_exit();
    }

    if (++key_presses > 1000) {
        fprintf(stderr, "image probe: the game keeps asking for a key\n");
        exit(1);
    }
    win->char_request = 0;
    ev->type = evtype_CharInput;
    ev->win = (winid_t)win;
    if (win->type == wintype_Graphics) {
        /* A title picture, waiting to be dismissed. */
        if (!script) {
            start_fingerprint();
            ImageProbeReport("title", NULL);
        }
        ev->val1 = keycode_Return;
    } else if (*keys) {
        ev->val1 = (unsigned char)*keys++;
    } else {
        ev->val1 = script ? script_key() : keycode_Return;
    }
}

// ---- Main ----------------------------------------------------------------------

int main(int argc, char **argv)
{
    int arg = 1;
    const char *script_path = NULL;
    while (arg + 1 < argc && (strcmp(argv[arg], "-d") == 0 || strcmp(argv[arg], "-s") == 0)) {
        if (argv[arg][1] == 'd')
            dump_dir = argv[arg + 1];
        else
            script_path = argv[arg + 1];
        arg += 2;
    }
    if (arg >= argc || arg + 2 < argc) {
        fprintf(stderr, "usage: %s [-d dumpdir] [-s script] <game file> [keys]\n", argv[0]);
        return 2;
    }
    if (arg + 1 < argc)
        keys = argv[arg + 1];

    gli_initialize_misc();
    gli_determinism = 1;
    null_stream = glk_stream_open_memory(NULL, 0, filemode_Write, 0);
    if (script_path) {
        script = fopen(script_path, "r");
        if (!script) {
            perror(script_path);
            return 2;
        }
        /* glk_stream_open_file() wants a fileref, and a fileref a file name. */
        stdout_stream = gli_new_stream(strtype_File, 0, 1, 0);
        stdout_stream->unicode = 0;
        stdout_stream->isbinary = 0;
        stdout_stream->file = stdout;
        stdout_stream->lastop = 0;
    }
    ImageProbeClear();

    char *game_argv[] = { argv[0], argv[arg], NULL };
    glkunix_startup_t startdata = { 2, game_argv };
    if (!glkunix_startup_code(&startdata))
        return 2;
    glk_main();
    image_exit();
    return 0;
}

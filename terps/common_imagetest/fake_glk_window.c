// The fake Glk window layer shared by image_glk.c (Plus / TaylorMade image
// probes) and scott/saga/test/scenetest.c. See fake_glk_window.h.
//
// Built without the rename headers: it calls the real (cheapglk) Glk.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "fake_glk_window.h"

fakeglk_window *fakeglk_windows[FAKEGLK_MAX_WINDOWS];
fakeglk_window *fakeglk_graphics;
uint32_t fakeglk_canvas[FAKEGLK_CANVAS_H][FAKEGLK_CANVAS_W];
strid_t fakeglk_text_stream, fakeglk_null_stream;
glui32 fakeglk_timer_ms;

static fakeglk_window pair_window;

#define windows fakeglk_windows
#define graphics fakeglk_graphics
#define canvas fakeglk_canvas
#define MAX_WINDOWS FAKEGLK_MAX_WINDOWS
#define CANVAS_W FAKEGLK_CANVAS_W
#define CANVAS_H FAKEGLK_CANVAS_H
#define UNSET FAKEGLK_UNSET

void fakeglk_clear_canvas(void)
{
    for (int y = 0; y < CANVAS_H; y++)
        for (int x = 0; x < CANVAS_W; x++)
            canvas[y][x] = UNSET;
}

void fakeglk_graphics_window_size(int *w, int *h)
{
    fakeglk_graphics_size(w, h);
    if (*w > CANVAS_W)
        *w = CANVAS_W;
    if (*h > CANVAS_H)
        *h = CANVAS_H;
}

winid_t fakeglk_window_open(winid_t split, glui32 method, glui32 size, glui32 wintype, glui32 rock)
{
    (void)split;
    fakeglk_window *win = calloc(1, sizeof *win);
    win->type = wintype;
    win->rock = rock;
    win->size = (method & winmethod_Fixed) ? size : 24;
    win->str = (wintype == wintype_TextBuffer) ? fakeglk_text_stream : fakeglk_null_stream;
    if (wintype == wintype_Graphics) {
        graphics = win;
        fakeglk_clear_canvas();
    }
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i]) {
            windows[i] = win;
            return (winid_t)win;
        }
    fprintf(stderr, "%s: too many windows\n", fakeglk_name);
    exit(2);
}

void fakeglk_window_close(winid_t w, stream_result_t *result)
{
    fakeglk_window *win = (fakeglk_window *)w;
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

winid_t fakeglk_window_iterate(winid_t w, glui32 *rock)
{
    int i = 0;
    if (w)
        for (i = 1; i <= MAX_WINDOWS; i++)
            if (windows[i - 1] == (fakeglk_window *)w)
                break;
    for (; i < MAX_WINDOWS; i++)
        if (windows[i]) {
            if (rock)
                *rock = windows[i]->rock;
            return (winid_t)windows[i];
        }
    return NULL;
}

void fakeglk_window_clear(winid_t w)
{
    if (w && (fakeglk_window *)w == graphics)
        fakeglk_clear_canvas();
}

void fakeglk_window_get_size(winid_t w, glui32 *width, glui32 *height)
{
    fakeglk_window *win = (fakeglk_window *)w;
    int gw = 80, gh = win ? (int)win->size : 0;
    if (win && win->type == wintype_Graphics)
        fakeglk_graphics_window_size(&gw, &gh);
    if (width)
        *width = gw;
    if (height)
        *height = gh;
}

winid_t fakeglk_window_get_parent(winid_t w)
{
    return w ? (winid_t)&pair_window : NULL;
}

void fakeglk_window_set_arrangement(winid_t w, glui32 method, glui32 size, winid_t key)
{
    (void)w;
    if (key && (method & winmethod_Fixed))
        ((fakeglk_window *)key)->size = size;
}

strid_t fakeglk_window_get_stream(winid_t w)
{
    return w ? ((fakeglk_window *)w)->str : NULL;
}

void fakeglk_window_set_echo_stream(winid_t w, strid_t str)
{
    (void)w; (void)str;
}

void fakeglk_set_window(winid_t w)
{
    glk_stream_set_current(w ? ((fakeglk_window *)w)->str : NULL);
}

void fakeglk_window_move_cursor(winid_t w, glui32 x, glui32 y)
{
    (void)w; (void)x; (void)y;
}

void fakeglk_window_set_background_color(winid_t w, glui32 color)
{
    (void)w; (void)color;
}

void fakeglk_window_fill_rect(winid_t w, glui32 color, glsi32 left, glsi32 top, glui32 width, glui32 height)
{
    if (!w || (fakeglk_window *)w != graphics)
        return;
    for (glsi32 y = top; y < top + (glsi32)height; y++)
        for (glsi32 x = left; x < left + (glsi32)width; x++)
            if (x >= 0 && x < CANVAS_W && y >= 0 && y < CANVAS_H)
                canvas[y][x] = color & 0xffffff;
}

glui32 fakeglk_style_measure(winid_t w, glui32 style, glui32 hint, glui32 *result)
{
    (void)w; (void)style; (void)hint; (void)result;
    return 0;
}

glui32 fakeglk_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 arrlen)
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

glui32 fakeglk_gestalt(glui32 sel, glui32 val)
{
    return fakeglk_gestalt_ext(sel, val, NULL, 0);
}

// ---- Events ----------------------------------------------------------------------

void fakeglk_request_char_event(winid_t w)
{
    if (w)
        ((fakeglk_window *)w)->char_request = 1;
}

void fakeglk_request_char_event_uni(winid_t w)
{
    fakeglk_request_char_event(w);
}

void fakeglk_cancel_char_event(winid_t w)
{
    if (w)
        ((fakeglk_window *)w)->char_request = 0;
}

void fakeglk_request_line_event_uni(winid_t w, glui32 *buf, glui32 maxlen, glui32 initlen)
{
    (void)initlen;
    fakeglk_window *win = (fakeglk_window *)w;
    if (!win)
        return;
    win->line_request = 1;
    win->linebuf = buf;
    win->linemax = maxlen;
}

void fakeglk_request_timer_events(glui32 millisecs)
{
    fakeglk_timer_ms = millisecs;
}

fakeglk_window *fakeglk_requesting_window(void)
{
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i] && (windows[i]->line_request || windows[i]->char_request))
            return windows[i];
    return NULL;
}

// ---- PNG -------------------------------------------------------------------------

static void png_chunk(FILE *f, const char *tag, const uint8_t *data, uint32_t len)
{
    uint8_t head[8] = { len >> 24, len >> 16, len >> 8, len, tag[0], tag[1], tag[2], tag[3] };
    uint32_t crc = crc32(crc32(0, head + 4, 4), data, len);
    uint8_t tail[4] = { crc >> 24, crc >> 16, crc >> 8, crc };
    fwrite(head, 1, 8, f);
    fwrite(data, 1, len, f);
    fwrite(tail, 1, 4, f);
}

void fakeglk_write_png(const char *path, int w, int h)
{
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

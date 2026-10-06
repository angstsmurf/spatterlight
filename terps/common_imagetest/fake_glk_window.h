// The fake Glk window layer shared by the picture harnesses that run a whole
// interpreter against a framebuffer: the image probes of the Plus and
// TaylorMade interpreters (image_glk.c) and the Scott scene tests
// (scott/saga/test/scenetest.c).
//
// The interpreter sources are built with a force-included rename header
// (image_glk.h, scott/saga/test/scene_glk.h) that maps the Glk window calls to
// the fakeglk_* functions below, and the event and exit calls to the
// harness's own functions. Streams, files and styles still come from
// cheapglk. Windows are bookkeeping only, except the graphics window, whose
// glk_window_fill_rect calls paint fakeglk_canvas.
//
// The harness supplies:
//   fakeglk_name            prefix of the layer's own error messages
//   fakeglk_graphics_size() the size the graphics window reports (it is
//                           clamped to the canvas here)
// and sets fakeglk_text_stream (the stream of a text buffer window; every
// other window writes to fakeglk_null_stream) before the game starts.

#ifndef FAKE_GLK_WINDOW_H
#define FAKE_GLK_WINDOW_H

#include <stdint.h>

#include "glk.h"

#define FAKEGLK_CANVAS_W 640
#define FAKEGLK_CANVAS_H 400
#define FAKEGLK_UNSET 0xffffffffu

typedef struct {
    glui32 type, rock, size;
    strid_t str;
    int char_request, line_request;
    glui32 *linebuf;    /* glk_request_line_event_uni */
    glui32 linemax;
} fakeglk_window;

#define FAKEGLK_MAX_WINDOWS 16
extern fakeglk_window *fakeglk_windows[FAKEGLK_MAX_WINDOWS];
extern fakeglk_window *fakeglk_graphics;
extern uint32_t fakeglk_canvas[FAKEGLK_CANVAS_H][FAKEGLK_CANVAS_W];
extern strid_t fakeglk_text_stream, fakeglk_null_stream;
extern glui32 fakeglk_timer_ms;

/* ---- Supplied by the harness ---- */
extern const char *fakeglk_name;
void fakeglk_graphics_size(int *width, int *height);

/* ---- Helpers for the harness ---- */

/* Mark every canvas pixel unpainted. */
void fakeglk_clear_canvas(void);

/* fakeglk_graphics_size(), clamped to the canvas. The caller presets *width
   and *height (the hook may leave them). */
void fakeglk_graphics_window_size(int *width, int *height);

/* The first window with a pending line or character request, or NULL. */
fakeglk_window *fakeglk_requesting_window(void);

/* Write the top-left w x h of the canvas as an RGB PNG at path; unpainted
   pixels come out mid-grey. */
void fakeglk_write_png(const char *path, int w, int h);

/* ---- The Glk calls the rename headers point at ---- */
winid_t fakeglk_window_open(winid_t split, glui32 method, glui32 size, glui32 wintype, glui32 rock);
void fakeglk_window_close(winid_t w, stream_result_t *result);
winid_t fakeglk_window_iterate(winid_t w, glui32 *rock);
void fakeglk_window_clear(winid_t w);
void fakeglk_window_get_size(winid_t w, glui32 *width, glui32 *height);
winid_t fakeglk_window_get_parent(winid_t w);
void fakeglk_window_set_arrangement(winid_t w, glui32 method, glui32 size, winid_t key);
strid_t fakeglk_window_get_stream(winid_t w);
void fakeglk_window_set_echo_stream(winid_t w, strid_t str);
void fakeglk_set_window(winid_t w);
void fakeglk_window_move_cursor(winid_t w, glui32 x, glui32 y);
void fakeglk_window_set_background_color(winid_t w, glui32 color);
void fakeglk_window_fill_rect(winid_t w, glui32 color, glsi32 left, glsi32 top, glui32 width, glui32 height);
glui32 fakeglk_style_measure(winid_t w, glui32 style, glui32 hint, glui32 *result);
glui32 fakeglk_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 arrlen);
glui32 fakeglk_gestalt(glui32 sel, glui32 val);

void fakeglk_request_char_event(winid_t w);
void fakeglk_request_char_event_uni(winid_t w);
void fakeglk_cancel_char_event(winid_t w);
void fakeglk_request_line_event_uni(winid_t w, glui32 *buf, glui32 maxlen, glui32 initlen);
void fakeglk_request_timer_events(glui32 millisecs);

#endif

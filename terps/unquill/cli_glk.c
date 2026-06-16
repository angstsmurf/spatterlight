/* cli_glk.c - a minimal stdio-backed Glk implementation for driving the
 * UnQuill interpreter from the command line, for testing.
 *
 * It implements just the subset of Glk that UnQuill uses: a single text
 * window written to stdout, char/line input read from stdin, and no-op
 * graphics. The result is a standalone CLI binary that reads commands from
 * stdin, handy for scripted testing of game loading and parsing.
 *
 * Build (from the repository root):
 *
 *   clang -Iglkimp -Iterps/decompressz80 -Iterps/unp64 -w -c \
 *       terps/unquill/unquill.c terps/unquill/playgame.c \
 *       terps/unquill/condact.c terps/unquill/tables.c \
 *       terps/unquill/cli_glk.c terps/decompressz80/decompressz80.c
 *   clang++ -std=c++17 -Iterps/unp64 -w -c \
 *       terps/unp64/unp64.cpp terps/unp64/6502_emu.cpp \
 *       terps/unp64/exo_util.cpp terps/unp64/globals.cpp \
 *       terps/unp64/scanners/*.cpp
 *   clang++ *.o -o unquill_cli
 *
 * Then:  printf '\n\n\nwest\ninventory\n' | ./unquill_cli GAME.t64
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"

/* gli_* globals referenced by UnQuill (only gli_determinism is used when
 * SPATTERLIGHT is not defined, but provide all three to be safe). */
glui32 gli_determinism = 1;   /* deterministic by default for reproducible tests */
int gli_enable_graphics = 0;
int gli_slowdraw = 0;

static int dummy_window;
#define WIN ((winid_t)&dummy_window)

void glk_exit(void) { fflush(stdout); exit(0); }

void glk_put_char(unsigned char ch) { putchar(ch); }
void glk_put_string(char *s) { fputs(s, stdout); }

void glk_set_window(winid_t win) { (void)win; }
void glk_window_clear(winid_t win) { (void)win; fputs("\n", stdout); }
void glk_window_close(winid_t win, stream_result_t *result)
{ (void)win; if (result) { result->readcount = 0; result->writecount = 0; } }

winid_t glk_window_open(winid_t split, glui32 method, glui32 size,
			glui32 wintype, glui32 rock)
{ (void)split; (void)method; (void)size; (void)wintype; (void)rock; return WIN; }

void glk_window_get_size(winid_t win, glui32 *widthptr, glui32 *heightptr)
{ (void)win; if (widthptr) *widthptr = 0; if (heightptr) *heightptr = 0; }

/* The status grid is unused in the CLI harness (glk_window_get_size reports
 * width 0, so the description falls back to inline buffer output). */
winid_t glk_window_get_parent(winid_t win) { (void)win; return WIN; }
void glk_window_set_arrangement(winid_t win, glui32 method, glui32 size, winid_t keywin)
{ (void)win; (void)method; (void)size; (void)keywin; }
void glk_window_move_cursor(winid_t win, glui32 xpos, glui32 ypos)
{ (void)win; (void)xpos; (void)ypos; }

void glk_window_fill_rect(winid_t win, glui32 color, glsi32 left, glsi32 top,
			  glui32 width, glui32 height)
{ (void)win; (void)color; (void)left; (void)top; (void)width; (void)height; }

void glk_window_set_background_color(winid_t win, glui32 color)
{ (void)win; (void)color; }

glui32 glk_style_measure(winid_t win, glui32 styl, glui32 hint, glui32 *result)
{ (void)win; (void)styl; (void)hint; (void)result; return 0; }

glui32 glk_gestalt(glui32 sel, glui32 val) { (void)sel; (void)val; return 0; }
glui32 glk_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 len)
{ (void)sel; (void)val; (void)arr; (void)len; return 0; }

void glk_request_timer_events(glui32 millisecs) { (void)millisecs; }

/* --- input --------------------------------------------------------------- */

static int   pending_char = 0;
static int   pending_line = 0;
static char *line_buf = NULL;
static glui32 line_cap = 0;

void glk_request_char_event(winid_t win) { (void)win; pending_char = 1; }
void glk_cancel_char_event(winid_t win) { (void)win; pending_char = 0; }
void glk_request_line_event(winid_t win, char *buf, glui32 maxlen, glui32 initlen)
{ (void)win; (void)initlen; pending_line = 1; line_buf = buf; line_cap = maxlen; }

/* Read one line of stdin (used for both char and line events so test input is
 * always line-oriented). */
static int read_line(char *buf, size_t cap)
{
    if (!fgets(buf, (int)cap, stdin))
	return 0;
    size_t n = strlen(buf);
    if (n && buf[n - 1] == '\n')
	buf[--n] = '\0';
    return 1;
}

void glk_select(event_t *event)
{
    memset(event, 0, sizeof(*event));
    event->win = WIN;

    if (pending_char)
    {
	char buf[256];
	pending_char = 0;
	if (!read_line(buf, sizeof(buf)))
	    glk_exit();
	event->type = evtype_CharInput;
	event->val1 = (buf[0] == '\0') ? keycode_Return : (glui32)(unsigned char)buf[0];
	return;
    }
    if (pending_line)
    {
	pending_line = 0;
	if (!read_line(line_buf, line_cap))
	    glk_exit();
	event->type = evtype_LineInput;
	event->val1 = (glui32)strlen(line_buf);
	return;
    }
    event->type = evtype_None;
}

/* --- file references (save/load): stub to a fixed filename ---------------- */

frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{ (void)usage; (void)fmode; (void)rock; return (frefid_t)&dummy_window; }
void glk_fileref_destroy(frefid_t fref) { (void)fref; }
char *glkunix_fileref_get_filename(frefid_t fref)
{ (void)fref; return "unquill-cli.sav"; }

/* --- streams: only enough for the "#transcript" command to link ----------- *
 * The CLI front end has no real Glk streams, so opening a transcript fails
 * gracefully (script_toggle reports it could not start one); on a real Glk
 * library the game is the one providing these. */

strid_t glk_stream_open_file(frefid_t fileref, glui32 fmode, glui32 rock)
{ (void)fileref; (void)fmode; (void)rock; return NULL; }
void glk_window_set_echo_stream(winid_t win, strid_t str)
{ (void)win; (void)str; }
void glk_stream_close(strid_t str, stream_result_t *result)
{ (void)str; if (result) { result->readcount = 0; result->writecount = 0; } }

/* --- entry point --------------------------------------------------------- */

int main(int argc, char **argv)
{
    glkunix_startup_t data;
    data.argc = argc;
    data.argv = argv;
    if (!glkunix_startup_code(&data))
	return 1;
    glk_main();
    return 0;
}

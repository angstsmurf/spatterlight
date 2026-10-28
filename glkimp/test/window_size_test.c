/*
 * glk_window_get_size counts the character cells that fit whole in a text
 * window, and a window split off to hold n cells gets the pixels for n
 * whole cells, so that it counts n again. Interpreters wrap their grid
 * lines and lay out status lines at that width, so a cell too many is a
 * line that wraps in the app, and a cell too few a column the game never
 * uses.
 *
 * window.c is built with stubs for the app (win_*) and the stream and
 * dispatch layers; the pixel boxes it hands win_sizewin are its own. The
 * test sweeps cell sizes, including the fractional ones of real fonts,
 * margins and window sizes, and checks:
 *
 *   - fixed splits: a grid or buffer window above or to the left of the
 *     root, n cells wide or tall, counts n;
 *   - every window: the n it counts fits in its pixels, and n + 1 doesn't.
 */

#include <stdio.h>

#include "glkimp.h"
#include "protocol.h"

/* What connect.c would get from the app's settings */
int gscreenw, gscreenh;
int gbuffermarginx, gbuffermarginy, ggridmarginx, ggridmarginy;
float gcellw, gcellh, gbufcellw, gbufcellh;
int gli_error_handling;

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock);
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode);
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock);

static int next_peer;
static char showed_error[256];

glui32 generate_tag(void) { return (glui32)next_peer; }
int win_newwin(int type) { (void)type; return next_peer++; }
void win_delwin(int name) { (void)name; }
void win_sizewin(int name, int x0, int y0, int x1, int y1)
{
    (void)name; (void)x0; (void)y0; (void)x1; (void)y1;
}
void win_showerror(const char *str)
{
    snprintf(showed_error, sizeof(showed_error), "%s", str);
}
void win_clear(int name) { (void)name; }
void win_moveto(int name, int x, int y) { (void)name; (void)x; (void)y; }
void win_print(int name, glui32 ch, int at) { (void)name; (void)ch; (void)at; }
void win_initchar(int name) { (void)name; }
void win_cancelchar(int name) { (void)name; }
void win_initline(int name, int cap, int len, void *buf)
{
    (void)name; (void)cap; (void)len; (void)buf;
}
int win_cancelline(int name, int cap, int unicode, void *buf)
{
    (void)name; (void)cap; (void)unicode; (void)buf;
    return 0;
}
void win_initmouse(int name) { (void)name; }
void win_cancelmouse(int name) { (void)name; }
void win_set_echo(int name, int val) { (void)name; (void)val; }
void win_set_terminators(int name, glui32 *keycodes, int count)
{
    (void)name; (void)keycodes; (void)count;
}

static stream_t a_stream;
stream_t *gli_stream_open_window(window_t *win) { (void)win; return &a_stream; }
void gli_delete_stream(stream_t *str) { (void)str; }
void gli_stream_fill_result(stream_t *str, stream_result_t *result)
{
    (void)str;
    if (result)
        result->readcount = result->writecount = 0;
}
void gli_stream_echo_line(stream_t *str, char *buf, glui32 len)
{
    (void)str; (void)buf; (void)len;
}
void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len)
{
    (void)str; (void)buf; (void)len;
}
void glk_stream_set_current(strid_t str) { (void)str; }

static long checks, failures;

static void fail(const char *what, double cell, int margin, int pixels,
                 glui32 got, glui32 want)
{
    if (failures++ < 20)
        printf("FAIL %s: cell %.4f margin %d pixels %d: counted %u, want %u\n",
               what, cell, margin, pixels, got, want);
}

/* The window's count n fits in its pixels, and n + 1 doesn't */
static void check_fits(const char *what, window_t *win, int vertical)
{
    glui32 w = 0, h = 0, n;
    grect_t *b = &win->bbox;
    int pixels, margin;
    double cell, room;

    glk_window_get_size(win, &w, &h);
    if (win->type == wintype_TextGrid)
    {
        cell = vertical ? gcellw : gcellh;
        margin = vertical ? ggridmarginx : ggridmarginy;
    }
    else
    {
        cell = vertical ? gbufcellw : gbufcellh;
        margin = vertical ? gbuffermarginx : gbuffermarginy;
    }
    pixels = vertical ? (int)(b->x1 - b->x0) : (int)(b->y1 - b->y0);
    room = pixels - 2 * margin;
    n = vertical ? w : h;
    ++checks;
    if (room < 0)
    {
        if (n != 0)
            fail(what, cell, margin, pixels, n, 0);
        return;
    }
    /* a thousandth of a cell is float error, as in gli_cells_in */
    if (n * cell > room + 0.002 * cell || (n + 1) * cell <= room - 0.002 * cell)
        fail(what, cell, margin, pixels, n, (glui32)(room / cell));
}

static void set_cells(double cell, int margin)
{
    gcellw = gbufcellw = (float)cell;
    gcellh = gbufcellh = (float)(cell * 2.1);
    ggridmarginx = gbuffermarginx = margin + 5;  /* + line fragment padding */
    ggridmarginy = gbuffermarginy = margin;
}

/* A grid or buffer window of n cells above or left of a buffer root */
static void check_fixed(glui32 type, glui32 dir)
{
    int vertical = (dir == winmethod_Left);
    const char *what = type == wintype_TextGrid
        ? (vertical ? "fixed grid width" : "fixed grid height")
        : (vertical ? "fixed buffer width" : "fixed buffer height");
    winid_t root, win;
    glui32 n;
    int m, step;

    gscreenw = 4000;
    gscreenh = 4000;
    set_cells(8, 0);
    root = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    win = glk_window_open(root, dir | winmethod_Fixed, 1, type, 0);

    for (step = 0; step <= 1500; ++step)
    {
        double cell = 5.0 + step * 0.01;

        for (m = 0; m <= 20; m += 5)
        {
            set_cells(cell, m);
            for (n = 1; n <= 160; ++n)
            {
                glui32 w = 0, h = 0;
                double cell_n = vertical ? (type == wintype_TextGrid ? gcellw : gbufcellw)
                                         : (type == wintype_TextGrid ? gcellh : gbufcellh);

                /* a window bigger than the screen gets what there is */
                if (gli_cells_span(n, cell_n) + 2 * (m + 5) > 4000)
                    break;
                glk_window_set_arrangement(glk_window_get_parent(win),
                                           dir | winmethod_Fixed, n, win);
                glk_window_get_size(win, &w, &h);
                ++checks;
                if ((vertical ? w : h) != n)
                    fail(what, vertical ? gcellw : gcellh,
                         vertical ? ggridmarginx : ggridmarginy,
                         vertical ? (int)(win->bbox.x1 - win->bbox.x0)
                                  : (int)(win->bbox.y1 - win->bbox.y0),
                         vertical ? w : h, n);
                check_fits(what, root, vertical);
            }
        }
    }
    glk_window_close(glk_window_get_root(), NULL);
}

/* A root window of every width and height */
static void check_root(glui32 type)
{
    const char *what = type == wintype_TextGrid ? "root grid" : "root buffer";
    winid_t root;
    int step, m;

    gscreenw = gscreenh = 100;
    set_cells(8, 0);
    root = glk_window_open(NULL, 0, 0, type, 0);
    for (step = 0; step <= 1500; step += 7)
    {
        double cell = 5.0 + step * 0.01;

        for (m = 0; m <= 20; m += 10)
        {
            set_cells(cell, m);
            for (gscreenw = 1; gscreenw <= 1600; ++gscreenw)
            {
                gscreenh = gscreenw;
                gli_windows_rearrange();
                check_fits(what, root, 1);
                check_fits(what, root, 0);
            }
        }
    }
    glk_window_close(glk_window_get_root(), NULL);
}

int main(void)
{
    check_fixed(wintype_TextGrid, winmethod_Left);
    check_fixed(wintype_TextGrid, winmethod_Above);
    check_fixed(wintype_TextBuffer, winmethod_Left);
    check_fixed(wintype_TextBuffer, winmethod_Above);
    check_root(wintype_TextGrid);
    check_root(wintype_TextBuffer);

    if (showed_error[0])
    {
        printf("FAIL: window.c complained: %s\n", showed_error);
        ++failures;
    }
    if (failures)
    {
        printf("window_size_test: %ld of %ld checks FAILED\n", failures, checks);
        return 1;
    }
    printf("window_size_test: ok (%ld checks)\n", checks);
    return 0;
}

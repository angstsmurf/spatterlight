/* oshtml.h -- HTML TADS markup to Glk */

#ifndef OSHTML_H
#define OSHTML_H

#include <stddef.h>

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "glk.h"

/*
 *   An <IMG> picture: the resource (osres_find), its Glk alignment, and its
 *   size, in pixels or with '_pct' set in per cent of the window; 0 if the
 *   tag doesn't give it
 */
typedef struct oshtml_image
{
    glui32 id;
    glui32 align;
    glui32 width, height;
    int width_pct, height_pct;
} oshtml_image;

/* Draw it in a text-buffer window */
void oshtml_draw_image(winid_t win, const oshtml_image *img);

/* and the size it's drawn at, in a window whose text is 'width' pixels */
int oshtml_image_size(const oshtml_image *img, glui32 width,
                      glui32 *w, glui32 *h);

/* Where a parser's text goes: the main window, or a banner */
typedef struct oshtml_sink
{
    void (*put)(void *ctx, const glui32 *buf, size_t len);

    /* the text's combination of OSSTY_* bits (osstyles.h) */
    void (*set_attr)(void *ctx, int combo);
    void (*set_color)(void *ctx, os_color_t fg, os_color_t bg);
    void (*set_screen_color)(void *ctx, os_color_t color);
    int (*width)(void *ctx);

    /*
     *   Does the window show this combination, which has OSSTY_CENTER,
     *   centred?  If not, the parser centres the line with spaces, and
     *   gives the sink the combination without OSSTY_CENTER.
     */
    int (*centers)(void *ctx, int combo);

    /*
     *   The text-buffer window to draw <IMG> pictures in, or null if the
     *   pictures can't be drawn there (and their ALT text is shown).  The
     *   sink puts out anything it holds back, such as newlines waiting for
     *   more text, as the picture goes after it.
     */
    winid_t (*window)(void *ctx);

    /* the text from now on is the link with this number, or none if 0 */
    void (*set_link)(void *ctx, glui32 link);

    /*
     *   A sink whose window() is null can still show pictures with this,
     *   keeping them with its text to be drawn again (the banners of the
     *   API); a tag banner draws them itself, to fit itself to them.  It
     *   returns false if it can't, and the ALT text is shown.  With a null
     *   picture it only answers whether it could.  Null in the main window.
     */
    int (*image)(void *ctx, const oshtml_image *img);

    /*
     *   The characters of an <HR> in these attributes across the window
     *   (osglk_rule_width).  width() counts the characters of text, to
     *   centre it and fill it out with spaces.
     */
    int (*rule_width)(void *ctx, int combo);
} oshtml_sink;

/*
 *   The links of <A HREF> (osglk.c).  The numbers are the same in every
 *   window, and a repeated href gets the same one.
 */
#define OSLINK_NOENTER 1        /* put the href on the command line */
#define OSLINK_APPEND 2         /* ...after what's there */

/* The number for an href, or 0 if it is shown as plain text */
glui32 osglk_link_add(const char *href, int flags);

/* Ask for link clicks in the windows that have links */
void osglk_tag_banner_request_links(void);
void osglk_api_banner_request_links(void);

/*
 *   Show the picture of a table's first or last cell beside the page, at
 *   the main window's left or right (osglk.c): false if it can't be
 */
int osglk_side_picture(int right, const oshtml_image *img);

/*
 *   Draw such a picture in a graphics window: as wide as it is, centred,
 *   and smaller if it's taller than the window
 */
void osglk_draw_side_picture(winid_t win, const oshtml_image *img,
                             glui32 bg);

/*
 *   Pixels, which Glk gives only for graphics windows: Spatterlight's own
 *   measures, or Gargoyle's extensions (osglk.c).  Without them the banners
 *   keep to characters.
 */
/* Gargoyle's window origins and cell sizes are in its overlay module */
#if defined(SPATTERLIGHT) \
    || (defined(GLK_MODULE_GARGLKWINSIZE) && defined(GLK_MODULE_GARGLKOVERLAY))
#define OSGLK_PIXELS

/* a window's width, inside a text buffer's margins */
glui32 osglk_px_width(winid_t win);

/* the width of the whole game window */
glui32 osglk_px_root_width(void);

/* the margins of a text buffer */
void osglk_px_margins(winid_t win, glui32 *x, glui32 *y);

/* the character cell of a text buffer, as wide as its size counts */
double osglk_px_cellw(void);
double osglk_px_cellh(void);

/* the lines a text buffer's text takes, a line of pictures counting one */
int osglk_px_lines(winid_t win);

/* the pixels its text takes, or 0 if we can't tell */
int osglk_px_text_height(winid_t win);
#endif

/* a window's background, zcolor_Default for the theme's */
void osglk_set_window_bg(winid_t win, glui32 color);

/*
 *   A text buffer banner at the top or bottom fitted to its text: as tall as
 *   the text's pixels, or 0 for the lines it was split off with.  A line in
 *   a font other than the buffer's may be taller than the buffer's lines, so
 *   counting lines would cut off the bottom.  Only Spatterlight can.
 */
void osglk_set_text_height(winid_t win, int pixels);

/*
 *   Text colours, with garglk_set_zcolors.  In Gargoyle a background is
 *   the paper of every window, so there only the screen colour sets one.
 */
void osglk_zcolors(glui32 fg, glui32 bg);
void osglk_zcolors_stream(strid_t str, glui32 fg, glui32 bg);

/*
 *   A table of nothing but pictures, laid out in pixels: each tile's place
 *   in it, and the link it is, or 0.  An area of an image map is a tile
 *   with no picture of its own, a link on the picture under it.
 */
#define OSHTML_TILES_MAX 48

typedef struct oshtml_tile
{
    oshtml_image img;
    glui32 x, y, w, h;
    glui32 link;
    int area;
} oshtml_tile;

typedef struct oshtml_pictable
{
    int ntiles;
    oshtml_tile tiles[OSHTML_TILES_MAX];
    glui32 width, height;
    int center;                 /* centred across the window */

    /*
     *   The whole banner, its pictures on its <BODY BACKGROUND>, tiled
     *   across the window, rather than a table under its text
     */
    int page;
    oshtml_image bg;

    /*
     *   A table cell's picture over the rest of the cell's text, rather
     *   than under the banner's: the cell's vertical alignment, 0 top, 1
     *   middle, 2 bottom, in the whole banner.  Arrival's map beside the
     *   page, over its "Remove Map".
     */
    int above;
    int valign;
} oshtml_pictable;

/*
 *   Is what the main window was sent since it was last cleared or waited on
 *   only a page of pictures on a <BODY BACKGROUND>?  If so, it is laid out
 *   in 'pt' (and forgotten, as is anything else it held).
 */
int oshtml_main_page(oshtml_pictable *pt);

/* The pixels of a line of text in a banner */
glui32 osglk_banner_line_pixels(void);

/*
 *   Show the table at the end of a tag banner under its text, as a picture
 *   whose tiles can be clicked (osglkban.c): false if it can't be
 */
int osglk_tag_banner_table(void *ctx, const oshtml_pictable *t);

/* The link of a click in a window of such a table, or 0 */
glui32 osglk_tag_banner_click(winid_t win, glui32 x, glui32 y);

/* Draw the tables again, as the windows changed */
void osglk_tag_banner_redraw(void);

/* Draw the pictures of the API side banners again, as the windows changed */
void osglk_api_banner_redraw(void);

/*
 *   Draw again those of them the interpreter's own rearranging resized,
 *   which no Redraw event tells of
 */
void osglk_api_banner_resized(void);

/* The box-drawing characters of a rule in this style across the window */
int osglk_rule_width(winid_t win, int combo);

typedef struct oshtml_parser oshtml_parser;

/* TADS 3 rules (t23run.cpp), before os_init */
void oshtml_set_tads3(int on);

/* A parser of a TADS 3 banner's own, in HTML mode */
oshtml_parser *oshtml_new(const oshtml_sink *sink, void *ctx);
void oshtml_delete(oshtml_parser *p);

/* Write text, in the game's character set */
void oshtml_write(oshtml_parser *p, const char *str, size_t len);

/* The formatter flushed the line without ending it */
void oshtml_flush_parser(oshtml_parser *p);

/* The window was cleared */
void oshtml_clear(oshtml_parser *p);

/* TADS 3: the formatter's own attributes and colours */
void oshtml_set_attr(oshtml_parser *p, int attr);
void oshtml_set_color(oshtml_parser *p, os_color_t fg, os_color_t bg);

/* The engine's \H+ and \H- (os_start_html, os_end_html) */
void oshtml_start(void);
void oshtml_end(void);

/* Write game text, in the game's character set, to the main window */
void oshtml_print_main(const char *str, size_t len);

/*
 *   Write game text to the status line: store at most 'max' characters of
 *   what it prints in 'out', and return the number stored.  With 'max' 0,
 *   this is the formatter's text for a hidden status line (os_status(2)).
 */
size_t oshtml_print_status(const char *str, size_t len,
                           glui32 *out, size_t max);

/*
 *   Write text of the OS layer's own (the input echo, the MORE prompt) to
 *   the main window as it is, after anything the parser holds back.  Like
 *   line input and clearing the window, it leaves the parser's idea of the
 *   line alone, as the formatter's is left alone: the formatter's first
 *   newline after a command still makes a blank line.
 */
void oshtml_put_plain(const char *str, size_t len);

/*
 *   os_flush in the main window: in TADS 2, show the spaces held back at the
 *   end of the line
 */
void oshtml_flush(void);

/* The main window was cleared */
void oshtml_clear_main(void);

/* Input is about to be read in the main window */
void oshtml_input(void);

/* a line of input was read, and a newline echoed */
void oshtml_input_done(void);

/* The formatter's text attributes (os_set_text_attr) */
void oshtml_set_os_attr(int attr);

/* The formatter's text colours (os_set_text_color) */
void oshtml_set_os_color(os_color_t fg, os_color_t bg);

/* The windows were rearranged, or the theme changed */
void oshtml_arrange(void);

/* TADS 3 rules, and whether HTML mode is on in the main window */
int oshtml_is_tads3(void);
int oshtml_is_html(void);

/* os_set_text_color itself (osglk.c) */
void osglk_set_text_color(os_color_t fg, os_color_t bg);

/*
 *   The main window's <BODY TEXT>.  In HTML TADS it colours the whole page,
 *   the text shown before it too, so in Spatterlight it's the window's own
 *   text colour, which a refresh puts on what's there; the text in it then
 *   goes out with no colour of its own (osglk_body_color).
 */
void osglk_set_body_text(os_color_t fg);
os_color_t osglk_body_color(os_color_t fg);

/*
 *   TADS 2's <BANNER> tags, which oshtml.c reads and osglkban.c shows.  A
 *   size is in pixels or per cent, or 'previous' (the banner's size so
 *   far), or not given (the size of its contents).
 */
enum { OSHTML_SIZE_NONE, OSHTML_SIZE_PREV, OSHTML_SIZE_PIX, OSHTML_SIZE_PCT };

#define OSHTML_BANNER_ID 64

typedef struct oshtml_banner
{
    char id[OSHTML_BANNER_ID];
    int align;                  /* OS_BANNER_ALIGN_xxx */
    int border;
    int height, height_units;
    int width, width_units;
} oshtml_banner;

/*
 *   Make the banner's window ready for its new contents, which replace the
 *   old: return the sink for them, or null if there's no window
 */
void *osglk_tag_banner_open(const oshtml_banner *ban,
                            const oshtml_sink **sink);

/*
 *   The contents are written: size the window to them, if it asked.  Returns
 *   nonzero if that pass only measured them, and they're to be written again.
 */
int osglk_tag_banner_done(void *ctx);

/* <BANNER REMOVE>, or with 'id' null, <BANNER REMOVEALL> */
void osglk_tag_banner_remove(const char *id);

#ifdef __cplusplus
}
#endif

#endif /* OSHTML_H */

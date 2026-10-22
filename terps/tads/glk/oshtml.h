/* oshtml.h -- HTML TADS markup to Glk */

#ifndef OSHTML_H
#define OSHTML_H

#include <stddef.h>

#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "glk.h"

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
     *   pictures can't be drawn there (and their ALT text is shown)
     */
    winid_t (*window)(void *ctx);

    /* the text from now on is the link with this number, or none if 0 */
    void (*set_link)(void *ctx, glui32 link);
} oshtml_sink;

/*
 *   The links of <A HREF> (osglk.c).  The numbers are the same in every
 *   window, and a repeated href gets the same one.
 */
#define OSLINK_NOENTER 1        /* put the href on the command line */
#define OSLINK_APPEND 2         /* ...after what's there */
#define OSLINK_PLAIN 4          /* no link at all */

/* The number for an href, or 0 if it is shown as plain text */
glui32 osglk_link_add(const char *href, int flags);

/* Ask for link clicks in the windows that have links */
void osglk_tag_banner_request_links(void);

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

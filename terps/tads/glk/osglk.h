/* osglk.h -- state the Glk interface's files share */

#ifndef OSGLK_H
#define OSGLK_H

#include "glk.h"

/* the main window and the status line, and their colours (osglk.c) */
extern winid_t mainwin;
extern winid_t statuswin;

extern glui32 mainfg;
extern glui32 mainbg;

extern glui32 statusfg;
extern glui32 statusbg;

/* game text to Unicode and back, in the game's character set (osparse.c) */
glui32 os_parse_chars(unsigned char *buf, glui32 buflen,
                      glui32 *out, glui32 outlen);
glui32 os_prepare_chars(glui32 *buf, glui32 buflen,
                        unsigned char *out, glui32 outlen);

/* draw the API banners again (osglkban.c) */
void os_banners_redraw(void);

/* the screen colour behind the main window's text (osglk.c) */
glui32 osglk_screen_bg(void);

/* the pictures beside the page, and a page of pictures (osglkpage.c) */
void osglk_side_pictures_draw(void);
void osglk_side_pictures_remove(void);
void osglk_page_show(void);
void osglk_page_redraw(void);
void osglk_page_remove(void);

/*
 *   The picture with an image map above the main window (osglkpage.c): drawn
 *   again, the link of a click on it or 0, its clicks asked for, and the
 *   line just read, which takes it away unless it came from one of its
 *   links ('link', or 0 for a line typed)
 */
void osglk_map_redraw(void);
glui32 osglk_map_click(winid_t win, glui32 x, glui32 y);
void osglk_map_request_mouse(void);
void osglk_map_line_read(glui32 link);

#endif /* OSGLK_H */

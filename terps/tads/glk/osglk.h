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

#endif /* OSGLK_H */

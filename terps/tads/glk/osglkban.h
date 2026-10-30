/* osglkban.h -- what the banners of the API (osglkban.c) and those of
   <BANNER> tags (osglktagban.c) share */

#ifndef OSGLKBAN_H
#define OSGLKBAN_H

#include "glk.h"
#include "oshtml.h"

#ifdef OSGLK_PIXELS
/*
 *   What the pictures in a text-buffer banner add to the size it fits its
 *   contents at.  Spatterlight counts a line with a picture in it as one
 *   line, however tall the picture, and a picture in the margin not at
 *   all.  Gargoyle's count has the lines its pictures take up, as it puts
 *   them on lines of their own, but not those beside a margin picture.
 */
typedef struct picture_fit_s
{
    int extra;                      /* lines the pictures make taller */
    int row, row_extra;             /* the last picture's line, its extra */
    int bottom;                     /* the line under the margin pictures */
    int unknown;                    /* a picture whose size isn't known */
    int last;                       /* nothing after the last picture */
    int count;                      /* the pictures */
}
picture_fit;

void picture_fit_init(picture_fit *fit);
int picture_fit_add(picture_fit *fit, winid_t win,
                    const oshtml_image *img, int line);
int picture_fit_lines(const picture_fit *fit, int lines);
#endif /* OSGLK_PIXELS */

/* a banner's grid has the main window's bold and italic while it opens */
void grid_hints_set(void);
void grid_hints_clear(void);

#endif /* OSGLKBAN_H */

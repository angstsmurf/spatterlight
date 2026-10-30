/* osglkpage.c -- pictures in windows of their own around the main
   window: beside its page, and a page of pictures on a background */

#include <string.h>

#include "glk.h"
#include "oshtml.h"
#include "osglk.h"

/* ------------------------------------------------------------------------ */
/*
 *   Pictures beside the page: the picture alone in a table's first or last
 *   cell, which HTML TADS shows as a border down the side of the text
 *   (1893).  Each goes in a graphics window at the main window's left or
 *   right, as wide as the picture, until the page is cleared.
 */
static struct
{
    winid_t win;
    oshtml_image img;
} side_pics[2];


void osglk_draw_side_picture(winid_t win, const oshtml_image *img,
                             glui32 bg)
{
    glui32 ww, wh, w, h;

    osglk_set_window_bg(win, bg);
    glk_window_clear(win);
    glk_window_get_size(win, &ww, &wh);
    if (!oshtml_image_size(img, ww, &w, &h) || !w || !h)
        return;

    /* the whole of it, if it's taller than the window */
    if (wh && h > wh)
    {
        w = w * wh / h;
        h = wh;
    }
    glk_image_draw_scaled(win, img->id,
                          (glsi32)(ww > w ? (ww - w) / 2 : 0), 0, w, h);
}

static void side_picture_draw(int right)
{
    if (side_pics[right].win)
        osglk_draw_side_picture(side_pics[right].win, &side_pics[right].img,
                                osglk_screen_bg());
}

void osglk_side_pictures_draw(void)
{
    side_picture_draw(0);
    side_picture_draw(1);
}

int osglk_side_picture(int right, const oshtml_image *img)
{
    glui32 mw, w, h;
    glui32 method = (right ? winmethod_Right : winmethod_Left)
        | winmethod_Fixed | winmethod_NoBorder;

    if (!mainwin || !glk_gestalt(gestalt_DrawImage, wintype_Graphics))
        return 0;

    /* not if it would leave the text too little room */
#ifdef OSGLK_PIXELS
    {
        glui32 mx = 0;
        osglk_px_margins(mainwin, &mx, NULL);
        mw = osglk_px_width(mainwin) + 2 * mx;
    }
#else
    mw = 0;
#endif
    if (!oshtml_image_size(img, mw, &w, &h) || !w || w * 4 > mw)
        return 0;

    right = right != 0;
    if (side_pics[right].win)
        glk_window_set_arrangement(glk_window_get_parent(side_pics[right].win),
                                   method, w, side_pics[right].win);
    else
        side_pics[right].win = glk_window_open(mainwin, method, w,
                                               wintype_Graphics, 0);
    if (!side_pics[right].win)
        return 0;
    side_pics[right].img = *img;
    side_picture_draw(right);
    return 1;
}

void osglk_side_pictures_remove(void)
{
    int i;

    for (i = 0 ; i < 2 ; ++i)
    {
        if (side_pics[i].win)
            glk_window_close(side_pics[i].win, NULL);
        side_pics[i].win = NULL;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   A page of pictures on a background picture, which is all the main
 *   window was sent before it waits for a key (the introduction of
 *   Macrocosm), fills the window as a graphics window until the key
 */
static winid_t page_win[2];
static oshtml_pictable page_pt;
static glui32 page_total, page_x;

/*
 *   The movie of the page is the picture in the main window's text, which
 *   animates there and not in a graphics window: the starfield is in a
 *   window on each side of it, as wide as what is left of the window
 */
static void main_page_draw(void)
{
    int i;

    for (i = 0 ; i < 2 ; ++i)
    {
        glui32 ww = 0, wh = 0, bw = 0, bh = 0, x, y;
        glui32 skip = i ? page_x : 0;

        osglk_set_window_bg(page_win[i], osglk_screen_bg());
        glk_window_clear(page_win[i]);
        glk_window_get_size(page_win[i], &ww, &wh);
        if (!page_pt.bg.id || !glk_image_get_info(page_pt.bg.id, &bw, &bh)
            || !bw || !bh)
            continue;

        /* the tiles run on across the windows, as one picture */
        for (y = 0 ; y < wh ; y += bh)
            for (x = 0 ; x < ww + bw ; x += bw)
                glk_image_draw(page_win[i], page_pt.bg.id,
                               (glsi32)(x - (i ? skip % bw : 0)),
                               (glsi32)y);
    }
}

static void main_page_place(void)
{
    glui32 w[3] = { 0, 0, 0 }, mx = 0, side;
    int i;

    glk_window_get_size(page_win[0], &w[0], NULL);
    glk_window_get_size(page_win[1], &w[2], NULL);
#ifdef OSGLK_PIXELS
    osglk_px_margins(mainwin, &mx, NULL);
    w[1] = osglk_px_width(mainwin) + 2 * mx;
#endif
    page_total = w[0] + w[1] + w[2];
    side = page_total > page_pt.width + 2 * mx + 8
        ? (page_total - page_pt.width - 2 * mx - 8) / 2 : 1;
    for (i = 0 ; i < 2 ; ++i)
        glk_window_set_arrangement(glk_window_get_parent(page_win[i]),
                                   (i ? winmethod_Right : winmethod_Left)
                                   | winmethod_Fixed | winmethod_NoBorder,
                                   side, page_win[i]);
    page_x = page_total - side;
}

void osglk_page_remove(void)
{
    int i;

    for (i = 0 ; i < 2 ; ++i)
    {
        if (page_win[i])
            glk_window_close(page_win[i], NULL);
        page_win[i] = NULL;
    }
}

void osglk_page_show(void)
{
    oshtml_pictable pt;
    int i;

    memset(&pt, 0, sizeof pt);
    if (!oshtml_main_page(&pt) || page_win[0]
#ifndef OSGLK_PIXELS
        || 1
#endif
        || !glk_gestalt(gestalt_DrawImage, wintype_Graphics))
        return;
    page_pt = pt;
    for (i = 0 ; i < 2 ; ++i)
        page_win[i] = glk_window_open(mainwin,
                                      (i ? winmethod_Right : winmethod_Left)
                                      | winmethod_Fixed | winmethod_NoBorder,
                                      1, wintype_Graphics, 0);
    if (!page_win[0] || !page_win[1])
    {
        osglk_page_remove();
        return;
    }
    main_page_place();
    main_page_draw();
}

/* the page of pictures again, if it's shown, at the window's new size */
void osglk_page_redraw(void)
{
    if (page_win[0])
    {
        main_page_place();
        main_page_draw();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   A picture with an image map in the page's text: School's map of the
 *   school, Arrival's of the ship.  In a text buffer the whole of a
 *   picture can be only one link, so it goes in a graphics window above
 *   the main window, as wide as the window lets it be and not more than
 *   MAP_ROOM per cent of its height, and a click on one of its areas is
 *   a click on that link.  It stays while the commands come from its
 *   links, so the player can go from place to place on it, and goes with
 *   the first one typed, a new page, or the next map.
 */
#define MAP_ROOM 60

static winid_t map_win;
static oshtml_pictable map_pt;
static glui32 map_x, map_w, map_h;     /* the picture as drawn, scaled */

/* the main window's height in pixels, beside a graphics window of no width */
static glui32 main_height(void)
{
    winid_t probe = glk_window_open(mainwin, winmethod_Left
                                    | winmethod_Fixed | winmethod_NoBorder,
                                    0, wintype_Graphics, 0);
    glui32 h = 0;

    if (!probe)
        return 0;
    glk_window_get_size(probe, NULL, &h);
    glk_window_close(probe, NULL);
    return h;
}

static void map_place(void)
{
    glui32 ww = 0, wh = 0, room, w = map_pt.width, h = map_pt.height;

    glk_window_get_size(map_win, &ww, &wh);
    room = (wh + main_height()) * MAP_ROOM / 100;
    if (ww && w > ww)
    {
        h = h * ww / w;
        w = ww;
    }
    if (room && h > room)
    {
        w = w * room / h;
        h = room;
    }
    if (!w || !h)
        w = h = 1;
    if (h != wh)
        glk_window_set_arrangement(glk_window_get_parent(map_win),
                                   winmethod_Above | winmethod_Fixed
                                   | winmethod_NoBorder, h, map_win);
    map_x = ww > w ? (ww - w) / 2 : 0;
    map_w = w;
    map_h = h;
}

static void map_draw(void)
{
    osglk_set_window_bg(map_win, osglk_screen_bg());
    glk_window_clear(map_win);
    glk_image_draw_scaled(map_win, map_pt.tiles[0].img.id, (glsi32)map_x, 0,
                          map_w, map_h);
}

int osglk_map_show(const oshtml_pictable *pt)
{
    if (!mainwin || !glk_gestalt(gestalt_DrawImage, wintype_Graphics)
        || !glk_gestalt(gestalt_MouseInput, wintype_Graphics))
        return 0;
    if (!map_win)
        map_win = glk_window_open(mainwin, winmethod_Above | winmethod_Fixed
                                  | winmethod_NoBorder, pt->height,
                                  wintype_Graphics, 0);
    if (!map_win)
        return 0;
    map_pt = *pt;
    map_place();
    map_draw();
    return 1;
}

void osglk_map_remove(void)
{
    if (map_win)
        glk_window_close(map_win, NULL);
    map_win = NULL;
}

void osglk_map_redraw(void)
{
    if (map_win)
    {
        map_place();
        map_draw();
    }
}

glui32 osglk_map_click(winid_t win, glui32 x, glui32 y)
{
    int i;

    if (!win || win != map_win || x < map_x || x >= map_x + map_w
        || y >= map_h)
        return 0;

    /* where on the picture, at its own size */
    x = (x - map_x) * map_pt.width / map_w;
    y = y * map_pt.height / map_h;
    for (i = 1 ; i < map_pt.ntiles ; ++i)
    {
        const oshtml_tile *a = &map_pt.tiles[i];
        if (a->link && x >= a->x && x < a->x + a->w
            && y >= a->y && y < a->y + a->h)
            return a->link;
    }
    return 0;
}

void osglk_map_request_mouse(void)
{
    if (map_win)
        glk_request_mouse_event(map_win);
}

void osglk_map_line_read(glui32 link)
{
    int i;

    for (i = 1 ; link && i < map_pt.ntiles ; ++i)
        if (map_pt.tiles[i].link == link)
            return;
    osglk_map_remove();
}

#include <math.h>
#include "glkimp.h"

#define LINE_FRAGMENT_PADDING (0)


/* yergh! the windows creation and arrangement with implicit
 * pair windows really really fucking stinks!
 * we do not expose it to the GUI server.
 * create peer windows only for the content windows.
 * do glk layout here and send resize commands.
 */

/* Linked list of all windows */
static window_t *gli_windowlist = NULL;
static window_t *gli_rootwin = NULL; /* The topmost window. */

static void gli_window_close(window_t *win, int recurse);
void gli_windows_rearrange(void);

/*
 * Create, destroy and arrange
 */

window_t *gli_new_window(glui32 type, glui32 rock)
{
    window_t *win = (window_t *)malloc(sizeof(window_t));
    if (!win) {
        gli_strict_warning("gli_new_window: malloc error. Failed to create new window");
        return NULL;
    }

    win->peer = -1;
    win->tag = generate_tag(); /* For serialization */

    switch (type)
    {
        case wintype_Blank:
            break;
        case wintype_TextGrid:
            win->peer = win_newwin(wintype_TextGrid);
            if (win->peer == -1)
            {
                gli_strict_warning("gli_new_window: failed to create peer window");
                free(win);
                return 0;
            }
            break;
        case wintype_TextBuffer:
            win->peer = win_newwin(wintype_TextBuffer);
            if (win->peer == -1)
            {
                gli_strict_warning("gli_new_window: failed to create peer window");
                free(win);
                return 0;
            }
            break;
        case wintype_Graphics:
            win->background = 0x00ffffff;
            win->peer = win_newwin(wintype_Graphics);
            if (win->peer == -1)
            {
                gli_strict_warning("gli_new_window: failed to create peer window");
                free(win);
                return 0;
            }
            break;
        case wintype_Pair:
            break;
        default:
            /* Unknown window type -- do not print a warning, just return 0
             to indicate that it's not possible. */
            free(win);
            return 0;
    }

    win->magicnum = MAGIC_WINDOW_NUM;
    win->rock = rock;
    win->type = type;

    win->echostr = NULL;

    win->parent = NULL; /* for now */
    memset(&win->pair, 0, sizeof(win->pair));
    memset(&win->line, 0, sizeof(win->line));

    win->char_request = FALSE;
    win->char_request_uni = FALSE;
    win->line_request = FALSE;
    win->line_request_uni = FALSE;
    win->mouse_request = FALSE;
    win->hyper_request = FALSE;
    win->echo_line_input = FALSE;
    win->line_terminators = NULL;
    win->termct = 0;
    win->line.buf = NULL;
    win->line.len = 0;

    win->style = style_Normal;

    win->str = gli_stream_open_window(win);
    win->echostr = NULL;

    win->prev = NULL;
    win->next = gli_windowlist;
    gli_windowlist = win;
    if (win->next)
        win->next->prev = win;

    if (gli_register_obj)
		win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);
	else
        win->disprock.ptr = NULL;

    return win;
}

void gli_delete_window(window_t *win)
{
    window_t *prev, *next;

    if (gli_rootwin == win)
        gli_rootwin = NULL;

    switch (win->type)
    {
        case wintype_Blank:
            break;
        case wintype_Pair:
            break;
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win_delwin(win->peer);
            if (win->line.buf)
            {
                if (gli_unregister_arr)
                {
                    (*gli_unregister_arr)(win->line.buf, win->line.cap,
                                          win->line_request_uni ? "&+#!Iu" : "&+#!Cn", win->line.inarrayrock);
                }
                win->line.buf = NULL;
            }
            break;
        case wintype_Graphics:
            win_delwin(win->peer);
            break;
    }

    if (gli_unregister_obj) {
        (*gli_unregister_obj)(win, gidisp_Class_Window, win->disprock);
        win->disprock.ptr = NULL;
    }

    win->magicnum = 0;

    /* Close window's stream. */
    if (win->str != NULL) {
        gli_delete_stream(win->str);
        win->str = NULL;
    }

    /* The window doesn't own its echostr; closing the window doesn't close
     the echostr. */
    win->echostr = NULL;

    prev = win->prev;
    next = win->next;
    win->prev = NULL;
    win->next = NULL;

    if (prev)
        prev->next = next;
    else {
        gli_windowlist = next;
    }
    if (next)
        next->prev = prev;
    
    free(win);
}

winid_t glk_window_open(winid_t splitwin,
                        glui32 method, glui32 size,
                        glui32 wintype, glui32 rock)
{
    window_t *newwin, *pairwin, *oldparent;
    glui32 val;

    if (!gli_rootwin)
    {
        if (splitwin)
        {
            gli_strict_warning("window_open: ref must be NULL");
            return 0;
        }

        /* ignore method and size now */
        oldparent = NULL;
    }

    else
    {
        if (!splitwin)
        {
            gli_strict_warning("window_open: ref must not be NULL");
            return 0;
        }

        val = (method & winmethod_DivisionMask);
        if (val != winmethod_Fixed && val != winmethod_Proportional)
        {
            gli_strict_warning("window_open: invalid method (not fixed or proportional)");
            return 0;
        }

        val = (method & winmethod_DirMask);
        if (val != winmethod_Above && val != winmethod_Below
            && val != winmethod_Left && val != winmethod_Right)
        {
            gli_strict_warning("window_open: invalid method (bad direction)");
            return 0;
        }

        oldparent = splitwin->parent;
        if (oldparent && oldparent->type != wintype_Pair)
        {
            gli_strict_warning("window_open: parent window is not Pair");
            return 0;
        }
    }

    newwin = gli_new_window(wintype, rock);
    if (!newwin)
    {
        gli_strict_warning("window_open: unable to create window");
        return 0;
    }

    if (!splitwin)
    {
        gli_rootwin = newwin;
    }
    else
    {
        /* create pairwin, with newwin as the key */
        pairwin = gli_new_window(wintype_Pair, 0);

        pairwin->pair.dir = method & winmethod_DirMask;
        pairwin->pair.division = method & winmethod_DivisionMask;
        pairwin->pair.key = newwin;
        pairwin->pair.size = size;
        pairwin->pair.vertical =
	    (pairwin->pair.dir == winmethod_Left ||
	     pairwin->pair.dir == winmethod_Right);
        pairwin->pair.backward =
	    (pairwin->pair.dir == winmethod_Left ||
	     pairwin->pair.dir == winmethod_Above);

        pairwin->pair.child1 = splitwin;
        pairwin->pair.child2 = newwin;

        splitwin->parent = pairwin;
        newwin->parent = pairwin;
        pairwin->parent = oldparent;

        if (oldparent)
        {
            if (oldparent->pair.child1 == splitwin)
                oldparent->pair.child1 = pairwin;
            else
                oldparent->pair.child2 = pairwin;
        }
        else
        {
            gli_rootwin = pairwin;
        }
    }

    gli_windows_rearrange();

    return newwin;
}

static void gli_window_close(window_t *win, int recurse)
{
    window_t *wx;

    for (wx=win->parent; wx; wx=wx->parent)
    {
        if (wx->type == wintype_Pair)
            if (wx->pair.key == win)
                wx->pair.key = NULL;
    }

    /* kill the children, if we're recursive */
    if (win->type == wintype_Pair && recurse)
    {
        if (win->pair.child1)
            gli_window_close(win->pair.child1, TRUE);
        if (win->pair.child2)
            gli_window_close(win->pair.child2, TRUE);
    }

    gli_delete_window(win);
}

void glk_window_close(window_t *win, stream_result_t *result)
{
    if (!win)
    {
        gli_strict_warning("window_close: invalid ref");
        return;
    }

    if (win == gli_rootwin || win->parent == NULL)
    {
        /* close the root window, which means all windows. */

        gli_rootwin = 0;

        /* begin (simpler) closation */

        gli_stream_fill_result(win->str, result);
        gli_window_close(win, TRUE);
    }

    else
    {
        /* have to jigger parent */
        window_t *pairwin, *sibwin, *grandparwin;

        pairwin = win->parent;
        if (win == pairwin->pair.child1)
            sibwin = pairwin->pair.child2;
        else if (win == pairwin->pair.child2)
            sibwin = pairwin->pair.child1;
        else
        {
            gli_strict_warning("window_close: window tree is corrupted");
            return;
        }

        grandparwin = pairwin->parent;
        if (!grandparwin)
        {
            gli_rootwin = sibwin;
            sibwin->parent = NULL;
        }
        else
        {
            if (grandparwin->pair.child1 == pairwin)
                grandparwin->pair.child1 = sibwin;
            else
                grandparwin->pair.child2 = sibwin;
            sibwin->parent = grandparwin;
        }

        /* Begin closation */

        gli_stream_fill_result(win->str, result);

        /* Close the child window (and descendants), so that key-deletion can
         crawl up the tree to the root window. */
        gli_window_close(win, TRUE);

        /* This probably isn't necessary, but the child *is* gone, so just
         in case. */
        if (win == pairwin->pair.child1) {
            pairwin->pair.child1 = NULL;
        }
        else if (win == pairwin->pair.child2) {
            pairwin->pair.child2 = NULL;
        }

        /* Now we can delete the parent pair. */
        gli_window_close(pairwin, FALSE);

        /* Sort out the arrangements */
        gli_windows_rearrange();
    }
}

void glk_window_get_arrangement(window_t *win, glui32 *method, glui32 *size, winid_t *keywin)
{
    glui32 val;

    if (!win)
    {
        gli_strict_warning("window_get_arrangement: invalid ref");
        return;
    }

    if (win->type != wintype_Pair)
    {
        gli_strict_warning("window_get_arrangement: not a Pair window");
        return;
    }

    val = win->pair.dir | win->pair.division;

    if (size)
        *size = win->pair.size;

    if (keywin)
    {
        if (win->pair.key)
            *keywin = win->pair.key;
        else
            *keywin = NULL;
    }

    if (method)
        *method = val;
}

void glk_window_set_arrangement(window_t *win, glui32 method, glui32 size, winid_t key)
{
    glui32 newdir;
    int newvertical, newbackward;

    if (!win)
    {
        gli_strict_warning("window_set_arrangement: invalid ref");
        return;
    }

    if (win->type != wintype_Pair)
    {
        gli_strict_warning("window_set_arrangement: not a Pair window");
        return;
    }

    if (key)
    {
        window_t *wx;
        if (key->type == wintype_Pair)
        {
            gli_strict_warning("window_set_arrangement: keywin cannot be a Pair");
            return;
        }
        for (wx=key; wx; wx=wx->parent)
        {
            if (wx == win)
                break;
        }
        if (wx == NULL)
        {
            gli_strict_warning("window_set_arrangement: keywin must be a descendant");
            return;
        }
    }

    newdir = method & winmethod_DirMask;
    newvertical = (newdir == winmethod_Left || newdir == winmethod_Right);
    newbackward = (newdir == winmethod_Left || newdir == winmethod_Above);
    if (!key)
        key = win->pair.key;

    if ((newvertical && !win->pair.vertical) || (!newvertical && win->pair.vertical))
    {
        if (!win->pair.vertical) {
            gli_strict_warning("window_set_arrangement: split must stay horizontal");
        } else {
            gli_strict_warning("window_set_arrangement: split must stay vertical");
        }
        return;
    }

    if (key && key->type == wintype_Blank
        && (method & winmethod_DivisionMask) == winmethod_Fixed)
    {
        gli_strict_warning("window_set_arrangement: a Blank window cannot have a fixed size");
        return;
    }

    if ((newbackward && !win->pair.backward) || (!newbackward && win->pair.backward))
    {
        /* switch the children */
        window_t *tmpwin = win->pair.child1;
        win->pair.child1 = win->pair.child2;
        win->pair.child2 = tmpwin;
    }

    /* set up everything else */
    win->pair.dir = newdir;
    win->pair.division = method & winmethod_DivisionMask;
    win->pair.key = key;
    win->pair.size = size;

    win->pair.vertical = (win->pair.dir == winmethod_Left || win->pair.dir == winmethod_Right);
    win->pair.backward = (win->pair.dir == winmethod_Left || win->pair.dir == winmethod_Above);

    gli_windows_rearrange();
}

void glk_window_get_size(window_t *win, glui32 *width, glui32 *height)
{
    glui32 wid = 0;
    glui32 hgt = 0;

    if (!win)
    {
        gli_strict_warning("window_get_size: invalid ref");
        return;
    }

//    fprintf(stderr, "glk_window_get_size: window %d\n", win->tag);

    switch (win->type)
    {
        case wintype_Blank:
        case wintype_Pair:
            /* always zero */
            break;
        case wintype_TextGrid:
            if (win->bbox.x0 + ggridmarginx * 2 + LINE_FRAGMENT_PADDING <= win->bbox.x1)
                wid = round(((win->bbox.x1 - win->bbox.x0) - ggridmarginx * 2 - LINE_FRAGMENT_PADDING) / gcellw);
            if (win->bbox.y0 + ggridmarginy * 2 <= win->bbox.y1)
            {
                hgt = round(((win->bbox.y1 - win->bbox.y0) - ggridmarginy * 2) / gcellh);
            }
            break;
        case wintype_TextBuffer:
            if (win->bbox.x0 + gbuffermarginx * 2 + LINE_FRAGMENT_PADDING <= win->bbox.x1)
                wid = round(((win->bbox.x1 - win->bbox.x0) - gbuffermarginx * 2 - LINE_FRAGMENT_PADDING) / gbufcellw);
            if (win->bbox.y0 + gbuffermarginy * 2 <= win->bbox.y1)
                hgt = round(((win->bbox.y1 - win->bbox.y0) - gbuffermarginy * 2) / gbufcellh);
//            fprintf(stderr, "wintype_TextBuffer: width: bbox.x1(%d) - bbox.x0:(%d) / gcellw (%f) = %d\n", win->bbox.x1, win->bbox.x0, gbufcellw, wid);
//            fprintf(stderr, "height: ( bbox.y1(%d) - bbox.y0:(%d) ) - gbuffermarginy (%d) * 2 / gbufcellh (%f) = %d\n", win->bbox.y1, win->bbox.y0, gbuffermarginy, gbufcellh, hgt);
            break;
        case wintype_Graphics:
            wid = win->bbox.x1 - win->bbox.x0;
            hgt = win->bbox.y1 - win->bbox.y0;
            break;
    }

    if (width)
        *width = wid;
    if (height)
        *height = hgt;
}

/*
 * Family matters
 */

window_t *gli_window_for_peer(int peer)
{
    window_t *win = gli_windowlist;
    while (win)
    {
        if (win->peer == peer)
            return win;
        win = win->next;
    }
    return NULL;
}

window_t *gli_window_for_tag(int tag)
{
    /* fprintf(stderr, "gli_window_for_tag: looking for window with tag %d\n", tag);*/

    window_t *win = gli_windowlist;
    while (win)
    {
        /* fprintf(stderr, "Found window with tag %d\n", win->tag); */
        if (win->tag == tag)
            return win;
        win = win->next;
    }
    return NULL;
}

winid_t glk_window_iterate(winid_t win, glui32 *rock)
{
    if (!win)
        win = gli_windowlist;
    else
        win = win->next;

    if (win)
    {
        if (rock)
            *rock = win->rock;
        return win;
    }

    if (rock)
        *rock = 0;
    return NULL;
}

glui32 glk_window_get_rock(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_rock: invalid ref.");
        return 0;
    }
    return win->rock;
}

window_t *gli_window_get(void)
{
    return gli_rootwin;
}

winid_t glk_window_get_root(void)
{
    if (!gli_rootwin) {
        for (window_t *win = glk_window_iterate(NULL, NULL); win; win = glk_window_iterate(win, NULL)) {
            if (win->parent == NULL && win->peer == -1) {
                gli_rootwin = win;
                return win;
            }
            if (win->parent == NULL) {
                fprintf(stderr, "glk_window_get_root: found window with no parent, but win->peer was %d\n", win->peer);
            }
        }
        fprintf(stderr, "glk_window_get_root: no root window found\n");
        if (gli_windowlist == NULL)
            fprintf(stderr, "because there are no windows\n");
        return NULL;
    }
    return gli_rootwin;
}

winid_t glk_window_get_parent(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_parent: invalid ref");
        return 0;
    }
    if (win->parent)
        return win->parent;
    else
        return 0;
}

winid_t glk_window_get_sibling(window_t *win)
{
    window_t *parwin;

    if (!win) {
        gli_strict_warning("window_get_sibling: invalid ref");
        return 0;
    }
    if (!win->parent)
        return 0;

    parwin = win->parent;
    if (parwin->pair.child1 == win)
        return parwin->pair.child2;
    else if (parwin->pair.child2 == win)
        return parwin->pair.child1;
    return 0;
}

glui32 glk_window_get_type(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_parent: invalid ref");
        return 0;
    }
    return win->type;
}

strid_t glk_window_get_stream(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_stream: invalid ref");
        return NULL;
    }
    if (win->str == NULL)
    {
        gli_strict_warning("window_get_stream: no windows stream!");
        win->str = gli_stream_open_window(win);
    }
    if (win->str->type != strtype_Window)
    {
        gli_strict_warning("window_get_stream: invalid stream type");
        win->str = gli_stream_open_window(win);
    }
    return win->str;
}

strid_t glk_window_get_echo_stream(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_get_echo_stream: invalid ref");
        return 0;
    }
    if (win->echostr)
        return win->echostr;
    else
        return 0;
}

void glk_window_set_echo_stream(window_t *win, stream_t *str)
{
    if (!win)
    {
        gli_strict_warning("window_set_echo_stream: invalid window id");
        return;
    }
    win->echostr = str;
}

void glk_set_window(window_t *win)
{
    if (!win)
        glk_stream_set_current(NULL);
    else {
        glk_stream_set_current(win->str);
    }
}

void gli_windows_unechostream(stream_t *str)
{
    window_t *win;
    for (win=gli_windowlist; win; win=win->next)
    {
        if (win->echostr == str)
            win->echostr = NULL;
    }
}

/*
 * Size changes, rearrangement and redrawing.
 */

void gli_window_rearrange(window_t *win, grect_t *box)
{
//    fprintf(stderr, "gli_window_rearrange win %d box x0:%u y0:%u x1:%u y1:%u\n", win->peer, box->x0, box->y0, box->x1, box->y1);
    win->bbox = *box;

    switch (win->type)
    {
        case wintype_Blank:
            break;

        case wintype_Pair:
        {
            grect_t box1, box2;
            int min, diff, split, splitwid, max;
            window_t *key;
            window_t *ch1, *ch2;

            if (win->pair.vertical)
            {
                min = win->bbox.x0;
                max = win->bbox.x1;
            }
            else
            {
                min = win->bbox.y0;
                max = win->bbox.y1;
            }
            diff = max - min;

            splitwid = 0; /* want border? */

            switch (win->pair.division)
            {
                case winmethod_Proportional:
                    split = (diff * win->pair.size) / 100;
                    break;
                case winmethod_Fixed:
                    /* Keeping track of the key may seem silly, since we don't really
                     use it when all sizes are measured in characters. But it's
                     good to know when it's invalid, so that the split can be set
                     to zero. It would suck if invalid keys seemed to work in
                     GlkTerm but not in GUI Glk libraries. */
                    key = win->pair.key;
                    if (!key)
                        split = 0;
                    else
                    {
                        switch (key->type)
                        {
                            case wintype_TextBuffer:
                                if (win->pair.size == 0)
                                    split = 0;
                                else if (win->pair.vertical)
                                    split = win->pair.size * gbufcellw + gbuffermarginx * 2 + LINE_FRAGMENT_PADDING;
                                else
                                    split = win->pair.size * gbufcellh + gbuffermarginy * 2;
                                break;
                            case wintype_TextGrid:
                                if (win->pair.size == 0) {
                                    split = 0;
                                } else if (win->pair.vertical) {
                                    split = win->pair.size * gcellw + ggridmarginx * 2 + LINE_FRAGMENT_PADDING;
                                } else {
                                    split = win->pair.size * gcellh + ggridmarginy * 2;
                                }
                                break;
                            case wintype_Graphics:
                                split = win->pair.size;
                                break;
                            default:
                                split = 0;
                                break;
                        }
                    }
                    break;
                default:
                    split = diff / 2;
                    break;
            }

            if (!win->pair.backward)
                split = max - split - splitwid;
            else
                split = min + split;

            if (min >= max)
                split = min;
            else
            {
                if (split < min)
                    split = min;
                else if (split > max - splitwid)
                    split = max - splitwid;
            }

            if (win->pair.vertical)
            {
                box1.x0 = win->bbox.x0;
                box1.x1 = split;
                box2.x0 = split + splitwid;
                box2.x1 = win->bbox.x1;
                box1.y0 = win->bbox.y0;
                box1.y1 = win->bbox.y1;
                box2.y0 = win->bbox.y0;
                box2.y1 = win->bbox.y1;
            }
            else
            {
                box1.y0 = win->bbox.y0;
                box1.y1 = split;
                box2.y0 = split + splitwid;
                box2.y1 = win->bbox.y1;
                box1.x0 = win->bbox.x0;
                box1.x1 = win->bbox.x1;
                box2.x0 = win->bbox.x0;
                box2.x1 = win->bbox.x1;
            }

            if (!win->pair.backward)
            {
                ch1 = win->pair.child1;
                ch2 = win->pair.child2;
            }
            else
            {
                ch1 = win->pair.child2;
                ch2 = win->pair.child1;
            }

            gli_window_rearrange(ch1, &box1);
            gli_window_rearrange(ch2, &box2);
        }
            break;

        case wintype_TextGrid:
            win_sizewin(win->peer, box->x0, box->y0, box->x1, box->y1);
            break;

        case wintype_TextBuffer:
            win_sizewin(win->peer, box->x0, box->y0, box->x1, box->y1);
            break;

        case wintype_Graphics:
            win_sizewin(win->peer, box->x0, box->y0, box->x1, box->y1);
            break;
    }
}

void gli_windows_rearrange(void)
{
    if (gli_rootwin)
    {
        grect_t box;
        box.x0 = 0;
        box.y0 = 0;
        box.x1 = gscreenw;
        box.y1 = gscreenh;
        gli_window_rearrange(gli_rootwin, &box);
    }
}

/*
 * Input events
 */

void gli_init_line_event(window_t *win, void *buf, int maxlen, int initlen)
{
    win_initline(win->peer, maxlen, initlen, buf);

    win->line.buf = buf;
    win->line.cap = maxlen;
    win->line.len = initlen;

    if (gli_register_arr)
        win->line.inarrayrock = (*gli_register_arr)(buf, maxlen, win->line_request_uni ? "&+#!Iu" : "&+#!Cn");

    if (win->line_request_uni)
        ((glui32*)buf)[initlen] = 0;
    else
        ((char*)buf)[initlen] = 0;
}

void gli_cancel_line_event(window_t *win, event_t *ev)
{
    win_cancelline(win->peer, win->line.cap, &win->line.len, win->line.buf);

    unsigned short *unicharbuf = (unsigned short *)win->line.buf;
    glui32 *tempbuf = malloc(win->line.len * sizeof(glui32));
    for (int i = 0; i < win->line.len; i++) {
        tempbuf[i] = unicharbuf[i];
    }

    if (win->line_request_uni)
    {
        glui32 *unicode = win->line.buf;
        for (int i = 0; i < win->line.len; i++)
            unicode[i] = tempbuf[i];
        if (win->echostr)
            gli_stream_echo_line_uni(win->echostr, win->line.buf, win->line.len);
    }
    else
    {
        unsigned char *latin1 = win->line.buf;
        for (int i = 0; i < win->line.len; i++)
            latin1[i] = tempbuf[i] < 0x100 ? tempbuf[i] : '?';
        if (win->echostr)
            gli_stream_echo_line(win->echostr, win->line.buf, win->line.len);
    }
    free(tempbuf);

    win->str->readcount +=  win->line.len;

    if (gli_unregister_arr)
        (*gli_unregister_arr)(win->line.buf, win->line.cap, win->line_request_uni ? "&+#!Iu" : "&+#!Cn", win->line.inarrayrock);

    if (ev == NULL)
        return;

    ev->type = evtype_LineInput;
    ev->win = win;
    ev->val1 = win->line.len;

    win->line.buf = NULL;
    win->line.len = 0;
    win->line.cap = 0;
}

void gli_window_set_root(window_t *win)
{
    if (win)
        gli_rootwin = win;
    else
        gli_strict_warning("gli_window_set_root: invalid ref");
}


void glk_request_char_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("request_char_event: invalid ref");
        return;
    }

    if (win->char_request || win->line_request)
    {
        gli_strict_warning("request_char_event: window already has keyboard request");
//        return;
    }

    switch (win->type)
    {
        case wintype_Graphics:
            /* extension */
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = TRUE;
            win->char_request_uni = FALSE;
            win_initchar(win->peer);
            break;
        default:
            gli_strict_warning("request_char_event: window does not support keyboard input");
            break;
    }
}

void glk_request_line_event(window_t *win, char *buf, glui32 maxlen, glui32 initlen)
{
    if (!win)
    {
        gli_strict_warning("request_line_event: invalid ref");
        return;
    }

//    fprintf(stderr, "%s window %d requesting  line event\n", win->type == wintype_TextGrid ? "Grid" : "Buffer", win->peer);

    if (win->char_request || win->line_request)
    {
        gli_strict_warning("request_line_event: window already has keyboard request");
//        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->line_request = TRUE;
            win->line_request_uni = FALSE;
            gli_init_line_event(win, buf, maxlen, initlen);
            break;
        default:
            gli_strict_warning("request_line_event: window does not support keyboard input");
            break;
    }

}

void glk_request_char_event_uni(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("request_char_event: invalid ref");
        return;
    }

    if (win->line_request)
    {
        gli_strict_warning("request_char_event_uni: window already has keyboard request");
        return;
    }

    if (win->char_request)
    {
        fprintf(stderr, "ERROR! request_char_event_uni: window %d already has character request\n",  win->peer);
    }

    switch (win->type)
    {
        case wintype_Graphics:
            /* extension */
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = TRUE;
            win->char_request_uni = TRUE;
            win_initchar(win->peer);
            break;
        default:
            gli_strict_warning("request_char_event: window does not support keyboard input");
            break;
    }
}

void glk_request_line_event_uni(window_t *win, glui32 *buf, glui32 maxlen, glui32 initlen)
{
    if (!win)
    {
        gli_strict_warning("request_line_event: invalid ref");
        return;
    }

    if (win->char_request || win->line_request)
    {
        gli_strict_warning("request_line_event_uni: window already has keyboard request");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->line_request = TRUE;
            win->line_request_uni = TRUE;
            gli_init_line_event(win, buf, maxlen, initlen);
            break;
        default:
            gli_strict_warning("request_line_event: window does not support keyboard input");
            break;
    }

}

void glk_set_echo_line_event(window_t *win, glui32 val)
{
    if (!win)
    {
        gli_strict_warning("set_echo_line_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
            win->echo_line_input = (val != 0);
            win_set_echo(win->peer, val);
            break;
        default:
            break;
    }
}

void glk_set_terminators_line_event(winid_t win, glui32 *keycodes, glui32 count)
{
    if (!win)
    {
        gli_strict_warning("set_terminators_line_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            break;
        default:
            gli_strict_warning("set_terminators_line_event: window does not support keyboard input");
            return;
    }

	win_set_terminators(win->peer, keycodes, count);

    if (win->line_terminators)
        free(win->line_terminators);

    if (!keycodes || count == 0)
    {
        win->line_terminators = NULL;
        win->termct = 0;
    }
    else
    {
        win->line_terminators = malloc((count + 1) * sizeof(glui32));
        if (win->line_terminators)
        {
            memcpy(win->line_terminators, keycodes, count * sizeof(glui32));
            win->line_terminators[count] = 0;
            win->termct = count;
        }
    }
}

int gli_window_check_terminator(glui32 ch)
{
    if (ch == keycode_Escape)
        return TRUE;
    else if (ch >= keycode_Func12 && ch <= keycode_Func1)
        return TRUE;
    else
        return FALSE;
}

void glk_request_mouse_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("request_mouse_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextGrid:
        case wintype_Graphics:
            win->mouse_request = TRUE;
            win_initmouse(win->peer);
            break;
        default:
            /* do nothing */
            break;
    }

    return;
}

void glk_cancel_char_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("cancel_char_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_Graphics:
            /* extension */
        case wintype_TextBuffer:
        case wintype_TextGrid:
            win->char_request = FALSE;
            win->char_request_uni = FALSE;
            win_cancelchar(win->peer);
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_line_event(window_t *win, event_t *ev)
{
    event_t dummyev;

    if (!ev)
        ev = &dummyev;

    gli_event_clearevent(ev);

    if (!win) {
        gli_strict_warning("cancel_line_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            if (win->line_request)
                gli_cancel_line_event(win, ev);
            win->line_request = FALSE;
            win->line_request_uni = FALSE;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_mouse_event(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("cancel_mouse_event: invalid ref");
        return;
    }

    switch (win->type) {
        case wintype_TextGrid:
        case wintype_Graphics:
            win->mouse_request = FALSE;
            win_cancelmouse(win->peer);
            break;
        default:
            /* do nothing */
            break;
    }

    return;
}

/*
 * Text output and cursor positioning
 */

void gli_window_put_char(window_t *win, unsigned chr)
{
    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
            if (chr <= 0xffff) {
                // This is a UCS-2 character
                win_print(win->peer, chr, win->style);
            } else if (chr <= 0x10ffff) {
                // This is a character that can be represented by a surrogate pair
                // 000uuuuuxxxxxxxxxxxxxxxx -> 110110wwwwxxxxxx 110111xxxxxxxxxx (wwww = uuuuu-1)
                win_flush();
                int w = (chr >> 16) - 1;
                int x = (chr & 0xffff);
                win_print(win->peer, 0xd800 | (w << 6) | (x >> 10), win->style);
                win_print(win->peer, 0xdc00 | (x & 0x3ff), win->style);
            } else {
                // This is a UCS-4 character outside the range of allowed unicode values
                win_print(win->peer, 0xfffd, win->style);
            }

            break;
    }
}


void glk_window_clear(window_t *win)
{
    if (!win)
    {
        gli_strict_warning("window_clear: invalid ref");
        return;
    }

    if (win->line_request)
    {
        gli_strict_warning("window_clear: window has pending line request");
        return;
    }

    win_clear(win->peer);
}

void glk_window_move_cursor(window_t *win, glui32 xpos, glui32 ypos)
{
    if (!win)
    {
        gli_strict_warning("window_move_cursor: invalid ref");
        return;
    }
    
    switch (win->type)
    {
        case wintype_TextGrid:
            win_moveto(win->peer, xpos, ypos);
            break;
        default:
            gli_strict_warning("window_move_cursor: not a TextGrid window");
            break;
    }
}

void gli_replace_window_list(window_t *newlist) /* Only used by autorestore */
{
    window_t *win;

    if (!newlist)
    {
        gli_strict_warning("gli_replace_window_list: invalid ref");
        return;
    }

    if (gli_windowlist)
    {
        win = glk_window_get_root();
        if (win)
            glk_window_close(win, NULL);

        /* At the time when this is called, the window list should be empty */
        while (gli_windowlist)
        {
            win = gli_windowlist;
            //#ifdef DEBUG
            fprintf(stderr, "gli_replace_window_list: Deleted a window with tag:%d \n",win->tag);
            //#endif /*DEBUG*/
            glk_window_close(win, NULL);
        }
    }
    
    gli_windowlist = newlist;
}

void gli_sanity_check_windows(void)
{
    if (gli_rootwin && !gli_windowlist)
        fprintf(stderr, "sanity_check: root window but no listed windows\n");
    if (!gli_rootwin && gli_windowlist)
    {
        fprintf(stderr, "sanity_check: no root window but listed windows.\n");
//        gli_window_set_root(gli_windowlist);
    }
    if (gli_rootwin && !gli_window_for_tag(gli_rootwin->tag))
        fprintf(stderr, "sanity_check: root window not listed\n");
    
    for (window_t *win = glk_window_iterate(NULL, NULL); win; win = glk_window_iterate(win, NULL)) {
        if (!win->type)
            fprintf(stderr, "sanity_check: window lacks type\n");
        
        if (!win->parent) {
            if (gli_rootwin && win != gli_rootwin) {
                fprintf(stderr, "sanity_check: window tag %d peer %d has no parent but is not rootwin. (Rootwin has tag %d)\n", win->tag, win->peer, gli_rootwin->tag);
            } else if (!gli_rootwin) {
                fprintf(stderr, "sanity_check: setting window tag %d peer %d as rootwin.\n", win->tag, win->peer);
                gli_window_set_root(win);
            }
        }
        else {
            if (win->parent && !win->parent->tag)
                fprintf(stderr, "sanity_check: window parent tag mismatch\n");
            if (win->parent->type != wintype_Pair)
                fprintf(stderr, "sanity_check: window parent is not pair\n");
        }
        if (!win->str)
            fprintf(stderr, "sanity_check: window lacks stream\n");
        if (win->str && win->tag != win->str->win->tag)
           fprintf(stderr, "sanity_check: window stream does not point pack at window %d, but at %d",win->tag, win->str->win->tag);
        if (win->str && (win->str->type != strtype_Window))
            fprintf(stderr, "sanity_check: window stream is wrong type\n");
        
        switch (win->type) {
            case wintype_Pair: {
                if (!win->pair.child1)
                    fprintf(stderr, "sanity_check: pair win has no child1\n");
                if (!win->pair.child2)
                    fprintf(stderr, "sanity_check: pair win has no child2\n");
                if (!win->pair.division)
                    fprintf(stderr, "sanity_check: pair win has no geometry\n");
                if (win->pair.child1 && !win->pair.child1->tag)
                    fprintf(stderr, "sanity_check: pair child1 tag mismatch\n");
                if (win->pair.child2 && !win->pair.child2->tag)
                    fprintf(stderr, "sanity_check: pair child2 tag mismatch\n");
                if (win->style)
                    fprintf(stderr, "sanity_check: pair window has styleset\n");
            }
                break;
                
            case wintype_TextBuffer: {
                if (!win->line.buf && win->line.cap)
                    fprintf(stderr, "sanity_check: buffer window has cap but no buf\n");
            }
                break;
                
            case wintype_TextGrid: {
                if (!win->line.buf && win->line.cap)
                    fprintf(stderr, "sanity_check: grid window has cap but no buf\n");
            }
                break;
        }
    }
}

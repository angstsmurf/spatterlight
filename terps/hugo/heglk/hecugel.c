/*
 * heuristics gone banana spanking mad!
 */

#include "heheader.h"
#include "glk.h"
#include "glkimp.h"
#include "fileref.h"

#undef fclose
#undef ferror
#undef fgetc
#undef fgets
#undef fread
#undef fprintf
#undef fputc
#undef fputs
#undef ftell
#undef fseek

#define LOG(...)
//fprintf(stderr, __VA_ARGS__)

#define ABS(a) ((a) < 0 ? -(a) : (a))
#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)


// Defined Hugo colors
#define HUGO_BLACK (0)
#define HUGO_BLUE (1)
#define HUGO_GREEN (2)
#define HUGO_CYAN (3)
#define HUGO_RED (4)
#define HUGO_MAGENTA (5)
#define HUGO_BROWN (6)
#define HUGO_WHITE (7)
#define HUGO_DARK_GRAY (8)
#define HUGO_LIGHT_BLUE (9)
#define HUGO_LIGHT_GREEN (10)
#define HUGO_LIGHT_CYAN (11)
#define HUGO_LIGHT_RED (12)
#define HUGO_LIGHT_MAGENTA (13)
#define HUGO_YELLOW (14)
#define HUGO_BRIGHT_WHITE (15)

#undef DEF_FCOLOR
#undef DEF_BGCOLOR
#undef DEF_SLFCOLOR
#undef DEF_SLBGCOLOR

#define DEF_FCOLOR 16
#define DEF_BGCOLOR 17
#define DEF_SLFCOLOR 18
#define DEF_SLBGCOLOR 19

// If the calculated screen width is greater than
// this number of chars, the status line will be
// one line rather than two.
#define MAX_STATUS_CHARS (81)

// We use a "fake" screen width (and height) in characters
// of 0x7FFF in order to disable the internal Hugo line wrapping,
// but sometimes calculations will make windows and lines a few
// characters less. So we use this arbitrarily lower number
// to check against.
#define INFINITE (0x7000)


#include <stdio.h>

#define MAXWINS 256

enum { ISSTATUS, ISAUX, ISMAIN };

struct winctx
{
    int l, t, b, r;        /* hugo coords */
    int x0, y0, x1, y1;    /* real, mapped, coords */
    winid_t win;           /* mapped peer window */
    int clear;             /* any printed content? */
    int isaux;             /* is aux?  */
    int curx, cury;        /* last cursor position */
    int fg, bg;            /* text colors in this window */
    int papercolor;        /* background color of this window */
    int halfscreenwidth;   /* is this window supposed to be half screen width?*/
    int peggedtoright;     /* is this window supposed to be attached to right edge?*/
    int wasmoved;          /* was this window moved when the player adjusted window size?*/
    int screenwidth_at_creation;
};

static struct winctx wins[MAXWINS]; // For simplicity, this starts at 1. Index 0 is not used.
static int nwins = 0;
static int curwin = 0;

static int screenwidth_in_chars = 0;
static int screenheight_in_chars = 0;


static int curstyle = style_Normal;
static int laststyle = style_Normal;

static int glk_fgcolor = -1;
static int glk_bgcolor = -1;
static int lastfg = -1;
static int lastbg = -1;
static int screen_bg = -1;

static int lastres = 0;

static int mainwin = 0;
static int statuswin = 0;
static int auxwin = 0;
static int menuwin = 0;

static int below_status = 0; // Typically main window
static int second_image_row = 0; // Typically a border image or line below the image area

static int guilty_bastards_graphics_win = 0;
static int guilty_bastards_aux_win = 0;

static int future_boy_main_image = 0;
static int future_boy_main_image_right = false;

static int menutop = 0;

static int inmenu = false;
static int mono = false;

static int iscolossal = false;
static int iscrimson = false;
static int iscwb2 = false;
static int isczk = false;
static int isenceladus = false;
static int isfallacy = false;
static int isfutureboy = false;
static int isguiltybastards = false;
static int ishtg = false;
static int ismarjorie = false;
static int isnecrotic = false;
static int isnextday = false;
static int istetris = false;
static int istraveling = false;
static int isworldbuilder = false;

static int showing_futureboy_title = false;
static int showing_author_photo = false;

static int keypress = 0;

static int just_displayed_something = false;
static int lowest_printed_position_in_status = 0;
static int in_arrange_event = false;
static int in_valid_window = false;

static int nlbufcnt;
static int nlbufwin;

#define CHARWIDTH 1

static void hugo_mapcurwin(void);
static void hugo_handlearrange(void);
static void hugo_flushnl(void);
static void heglk_sizeifexists(int i);
static void heglk_zcolor(void);
static int heglk_push_down_main_window_to(int y);
static void heglk_ensure_menu(void);
static int heglk_find_attached_at_top(int top);
static int heglk_find_attached_to_left(int left);
static void heglk_adjust_guilty_bastards_windows(void);

#define MAXRES 1024

static schanid_t mchannel = NULL;
static schanid_t schannel = NULL;

static long resids[MAXRES];
static int numres = 0;


/* glk_main
 *
 * The entry point for Glk:  starts up the Hugo Engine.
 * The arguments glk_argc and glk_argv must be made up before
 * glk_main() is called.
 */

void glk_main(void)
{
    he_main(0, NULL); /* no argc, argv */
}

void heglk_printfatalerror(char *err)
{
    LOG("hugo: %s\n", err);
}

/* Convert Glk special keycodes: */
int hugo_convert_key(int n)
{
    switch (n)
    {
        case keycode_Left:     n = 8;     break;
        case keycode_Right:    n = 21;    break;
        case keycode_Up:       n = 11;    break;
        case keycode_Down:     n = 10;    break;
        case keycode_Return:   n = 13;    break;
        case keycode_Escape:   n = 27;    break;
    }

    return n;
}

/* hugo_timewait
 * Waits for 1/n seconds.  Returns false if waiting is unsupported.
 */

int hugo_timewait(int n)
{
    glui32 millisecs;
    event_t ev = {0};

    if (!glk_gestalt(gestalt_Timer, 0))
        return false;

    if (n == 0)
        return true;

    millisecs = 1000 / n;
    if (millisecs == 0)
        millisecs = 1;

    if (keypress == 0 && wins[curwin].win && wins[curwin].win->char_request == false)
        glk_request_char_event(wins[curwin].win);

    glk_request_timer_events(millisecs);
    while (ev.type != evtype_Timer)
    {
        glk_select(&ev);
        if (ev.type == evtype_Arrange)
            hugo_handlearrange();
        if (ev.type == evtype_CharInput)
        {
            keypress = hugo_convert_key(ev.val1);
        }
    }
    glk_request_timer_events(0);

    if (!istetris && wins[curwin].win && wins[curwin].win->char_request)
        glk_cancel_char_event(wins[curwin].win);

    return true;
}


/* ----------------------------------------------------------------------------- */

/*
 * Stubs, trivials, and no-ops.
 */

void *hugo_blockalloc(long num)
{
    return malloc(num * sizeof(char));
}

void hugo_blockfree(void *block)
{
    free(block);
}

void hugo_closefiles()
{
    /* Glk closes all files at glk_exit() */
}

void hugo_setgametitle(char *t)
{
    if (!strncmp(t, "NOT READY YET", 13))
        return;

    garglk_set_story_title(t);

    if (!strncmp(t, "Hugo Tetris", 11)) {
        istetris = true;
    } else if (!strncmp(t, "CRYPTOZOOKEEPER", 15)) {
        isczk = true;
    } else if (!strncmp(t, "Future Boy!", 11)) {
        isfutureboy = true;
    } else if (!strncmp(t, "ENCELADUS", 9)) {
        isenceladus = true;
    } else if (!strncmp(t, "NECROTIC DRIFT", 14)) {
        isnecrotic = true;
    } else if (!strncmp(t, "FALLACY OF DAWN", 15)) {
        isfallacy = true;
    } else if (!strncmp(t, "The Clockwork Boy 2", 19)) {
        iscwb2 = true;
    } else if (!strncmp(t, "World Builder", 13)) {
        isworldbuilder = true;
    } else if (!strncmp(t, "The Next Day", 12)) {
        isnextday = true;
    } else if (!strncmp(t, "A CRIMSON", 9)) {
        iscrimson = true;
    } else if (!strncmp(t, "Guilty Bastards", 15)) {
        isguiltybastards = true;
    } else if (!strncmp(t, "Tales of the Traveling", 22)) {
        istraveling = true;
    }
}

int hugo_hasvideo(void)
{
    return false;
}

int hugo_playvideo(HUGO_FILE infile, long reslength,
                   char loop_flag, char background, int volume)
{
    glk_stream_close(infile, NULL);
    return true;
}

void hugo_stopvideo(void) {}

/*
 * Sound & music
 */

static int loadres(HUGO_FILE infile, int reslen)
{
    char *buf, *origbuf;
    long offset, suboffset;
    int id;
    int i;

    offset = glk_stream_get_position(infile);
    for (i = 0; i < numres; i++)
        if (resids[i] == offset) {
            return i;
        }

    /* Too many resources loaded... */
    if (numres + 1 == MAXRES)
        return -1;

    id = numres++;

    resids[id] = offset;
    origbuf = malloc(reslen);
    buf = origbuf;

    glk_get_buffer_stream(infile, (char *)buf, reslen);

    suboffset = 0;

    // At least Hugo Tetris sends malformed data with extra junk before the RIFF header.
    // We try to fix that here.
    if (reslen > 128)
    {
        for (i = 0; i < 124; i++)
        {
            if (!memcmp(buf + i, "RIFF", 4))
            {
                suboffset = i;
                break;
            }
        }
    }

    buf += suboffset;

    glui32 type = gli_detect_sound_format(buf, reslen);
    gli_set_sound_resource(id, type, buf, reslen, infile->filename, offset + suboffset);

    free(origbuf);
    return id;
}

void initsound()
{
    if (!glk_gestalt(gestalt_Sound, 0))
        return;
    schannel = glk_schannel_create(0);
}

void initmusic()
{
    if (!glk_gestalt(gestalt_Sound, 0) || !glk_gestalt(gestalt_SoundMusic, 0))
        return;
    mchannel = glk_schannel_create(0);
}

int hugo_playmusic(HUGO_FILE infile, long reslen, char loop_flag)
{
    int id;

    if (!mchannel)
        initmusic();
    if (mchannel)
    {
        id = loadres(infile, reslen);
        if (id < 0)
        {
            glk_stream_close(infile, NULL);
            return false;
        }
        glk_schannel_play_ext(mchannel, id, loop_flag ? -1 : 1, 0);
    }

    glk_stream_close(infile, NULL);
    return true;
}

void hugo_musicvolume(int vol)
{
    if (!mchannel) initmusic();
    if (!mchannel) return;
    glk_schannel_set_volume(mchannel, (vol * 0x10000) / 100);
}

void hugo_stopmusic(void)
{
    if (!mchannel) initmusic();
    if (!mchannel) return;
    glk_schannel_stop(mchannel);
}

int hugo_playsample(HUGO_FILE infile, long reslen, char loop_flag)
{
    int id;

    if (schannel)
    {
        id = loadres(infile, reslen);
        if (id < 0)
        {
            glk_stream_close(infile, NULL);
            return false;
        }
        glk_schannel_play_ext(schannel, id, loop_flag ? -1 : 1, 0);
    }

    glk_stream_close(infile, NULL);
    return true;
}

void hugo_samplevolume(int vol)
{
    if (!schannel) initsound();
    if (!schannel) return;
    glk_schannel_set_volume(schannel, (vol * 0x10000) / 100);
}

void hugo_stopsample(void)
{
    if (!schannel) initsound();
    if (!schannel) return;
    glk_schannel_stop(schannel);
}

/*
 * Return true if a keypress is waiting to be retrieved.
 */

int hugo_iskeywaiting(void)
{
    return keypress;
}


void hugo_scrollwindowup()
{
    /* Glk should look after this for us */
}

int hugo_getkey(void)
{
    return hugo_waitforkey();
}

void hugo_settextmode(void)
{
    /* This function does whatever is necessary to set the system up for
     * a standard text display
     */
    charwidth = FIXEDCHARWIDTH;
    lineheight = FIXEDLINEHEIGHT;
}



int hugo_color(int c)
{
    int converted = 0;
    /* Color-setting functions should always pass the color through
     hugo_color() in order to properly set default fore/background colors:
     */

    switch (c)
    {
        case DEF_FCOLOR:     converted = gfgcol; break;
        case DEF_BGCOLOR:     converted = gbgcol; break;
        case DEF_SLFCOLOR:     converted = gsfgcol; break;
        case DEF_SLBGCOLOR:     converted = gsbgcol; break;
        case 20:
            converted = hugo_color(fcolor); break;    /* match foreground */
        case HUGO_BLACK: converted = 0x000000;  break;
        case HUGO_BLUE: converted = 0x00007f;  break;
        case HUGO_GREEN: converted = 0x007f00;  break;
        case HUGO_CYAN: converted = 0x007f7f;  break;
        case HUGO_RED: converted = 0x7f0000;  break;
        case HUGO_MAGENTA: converted = 0x7f007f;  break;
        case HUGO_BROWN: converted = 0x7f5f00;  break;
        case HUGO_WHITE: converted = 0xcfcfcf;  break;
        case HUGO_DARK_GRAY: converted = 0x3f3f3f;  break;
        case HUGO_LIGHT_BLUE: converted = 0x0000ff;  break;
        case HUGO_LIGHT_GREEN: converted = 0x00ff00;  break;
        case HUGO_LIGHT_CYAN: converted = 0x00ffff;  break;
        case HUGO_LIGHT_RED: converted = 0xff0000;  break;
        case HUGO_LIGHT_MAGENTA: converted = 0xff00ff;  break;
        case HUGO_YELLOW: converted = 0xffff00;  break;
        case HUGO_BRIGHT_WHITE: converted = 0xffffff; break;
        default:
            LOG("Unmapped color %d\n", c);
    }

    if (wins[curwin].win || (statuswin && curwin == statuswin))
    {
        if (curwin == statuswin || wins[curwin].win->type == wintype_TextGrid)
        {
            if (c == DEF_SLFCOLOR && wins[curwin].fg == DEF_SLFCOLOR && wins[curwin].bg == DEF_SLBGCOLOR)
            {
                return zcolor_Default;
            }
            if (c == DEF_SLBGCOLOR && wins[curwin].bg == DEF_SLBGCOLOR) { //} && !inmenu) {
                return zcolor_Default;
            }
        } else {
            if (c == DEF_FCOLOR  && wins[curwin].fg == DEF_FCOLOR && wins[curwin].bg == DEF_BGCOLOR)
            {
                return zcolor_Default;
            }
            if (c == DEF_BGCOLOR && wins[curwin].bg == DEF_BGCOLOR)
            {
                return zcolor_Default;
            }
        }
    }

    return converted;
}

void print_colorname(int c)
{
    switch(c) {
        case DEF_FCOLOR:
            LOG("This is the default main window text color\n");
            break;
        case DEF_BGCOLOR:
            LOG("This is the default main window background color\n");
            break;
        case DEF_SLFCOLOR:
            LOG("This is the default status window text color\n");
            break;
        case DEF_SLBGCOLOR:
            LOG("This is the default status window background color\n");
            break;
        default:
            break;
    }
}

void hugo_settextcolor(int c)
{
//    LOG("hugo_settextcolor %d Glk:(%x, %d) \n", c, hugo_color(c), hugo_color(c));
    print_colorname(c);
    if (curwin) {
        wins[curwin].fg = c;
    } else {
//        LOG("hugo_settextcolor: No curwin\n");
    }
}


void hugo_setbackcolor(int c)
{
//    LOG("hugo_setbackcolor %d Glk:(%x, %d) \n", c, hugo_color(c), hugo_color(c));
//    print_colorname(c);

    if (curwin)
    {
        wins[curwin].bg = c;
    } else  {
//        LOG("hugo_setbackcolor: No curwin\n");
    }
}




/* CHARACTER AND TEXT MEASUREMENT

 For non-proportional printing, screen dimensions will be given
 in characters, not pixels.

 For proportional printing, screen dimensions need to be in
 pixels, and each width routine must take into account the
 current font and style.

 The hugo_strlen() function is used to give the length of
 the string not including any non-printing control characters.
 */

int hugo_charwidth(char a)
{
    /*  As given here, this function works only for non-proportional
     printing.  For proportional printing, hugo_charwidth() should
     return the width of the supplied character in the current
     font and style.
     */

    if (a==FORCED_SPACE)
        return CHARWIDTH;         /* same as ' ' */

    else if ((unsigned char)a >= ' ') /* alphanumeric characters */

        return CHARWIDTH;         /* for non-proportional */

    return 0;
}

int hugo_textwidth(char *a)
{
    int i, slen, len = 0;

    slen = (int)strlen(a);

    for (i=0; i<slen; i++)
    {
        if (a[i]==COLOR_CHANGE) i+=2;
        else if (a[i]==FONT_CHANGE) i++;
        else
            len += hugo_charwidth(a[i]);
    }

    return len;
}

int hugo_strlen(char *a)
{
    int i, slen, len = 0;

    slen = (int)strlen(a);

    for (i=0; i<slen; i++)
    {
        if (a[i]==COLOR_CHANGE) i+=2;
        else if (a[i]==FONT_CHANGE) i++;
        else len++;
    }

    return len;
}

/* heglk_get_linelength
 *
 * Specially accommodated in GetProp() in heobject.c.
 * While the engine thinks that the linelength is 0x7fff,
 * this tells things like the display object the actual
 * length.  (Defined as ACTUAL_LINELENGTH in heheader.h.)
 */

int heglk_get_linelength(void)
{
    glui32 width = gscreenw;
    width -= 2 * ggridmarginx;
    width = width / gcellw;
    return width - 2; /* -2 to trigger automatic line wrapping */
}

int heglk_get_screenheight(void)
{
    glui32 height = (gscreenh - 2 * ggridmarginy) / gcellh;
    return height;
}

/* ----------------------------------------------------------------------------- */

void hugo_init_screen(void)
{
    // LOG("hugo_init_screen\n");

    /* Does whatever has to be done to initially set up the display. */

    /* By setting the width and height so high, we're basically
     forcing the Glk library to deal with text-wrapping and
     page ends
     */

    SCREENWIDTH = 0x7fff;
    SCREENHEIGHT = 0x7fff;
    FIXEDCHARWIDTH = 1;
    FIXEDLINEHEIGHT = 1;

    screenwidth_in_chars = heglk_get_linelength();
    screenheight_in_chars = heglk_get_screenheight();

    nwins = 1;
    curwin = 0;

    nlbufwin = 0;
    nlbufcnt = 0;

    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Justification, stylehint_just_Centered);
    //    glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_TextColor, gsfgcol);
    //    glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_BackColor, gsbgcol);
    //    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, hugo_color(fcolor));

    game_version = HEVERSION * 10 + HEREVISION;

    hugo_clearfullscreen();

    if (mainwin)
        wins[mainwin].bg = HUGO_BLACK;

    hugo_clearwindow();

    win_setbgnd(-1, 0);

    if (!strncmp(gli_story_name, "colossal.hex", 12))
    {
        iscolossal = true;
    } else if (!strncmp(gli_story_name, "htgessay.hex", 12))
    {
        ishtg = true;
    } else if (!strncmp(gli_story_name, "marjorie.hex", 12))
    {
        ismarjorie = true;
    }
}

void hugo_cleanup_screen(void)
{
    /* Does whatever has to be done to clean up the display pre-termination. */
}

void heglk_record_physical(struct winctx ctx) {
    /* record position of the window */
    physical_windowleft = (ctx.l-1)*FIXEDCHARWIDTH;
//    LOG("physical_windowleft: %d\n", physical_windowleft);
    physical_windowtop = (ctx.t-1)*FIXEDLINEHEIGHT;
//    LOG("physical_windowtop: %d\n", physical_windowtop);
    physical_windowright = ctx.r*FIXEDCHARWIDTH-1;
//    LOG("physical_windowright: %d\n", physical_windowright);
    physical_windowbottom = ctx.b*FIXEDLINEHEIGHT-1;
//    LOG("physical_windowbottom: %d\n", physical_windowbottom);
    if (ctx.r - ctx.l < screenwidth_in_chars)
        physical_windowwidth = (ctx.r-ctx.l-(ggridmarginx / gcellw - 1))*FIXEDCHARWIDTH;
    else
        physical_windowwidth = (ctx.r-ctx.l+1)*FIXEDCHARWIDTH;
    if (ctx.isaux && ctx.l > 3 && ctx.r > screenwidth_in_chars - 2)
        physical_windowwidth = SCREENWIDTH;

//    LOG("physical_windowwidth: %d\n", physical_windowwidth);
    physical_windowheight = (ctx.b-ctx.t+1)*FIXEDLINEHEIGHT;
//    LOG("physical_windowheight: %d\n", physical_windowheight);
}

/*
 * Get a line of input from the keyboard, storing it in <buffer>.
 */

void hugo_getline(char *prompt)
{
    event_t ev = {0};

    LOG("getline '%s'\n", prompt);

    /* make sure we have a window */
    hugo_mapcurwin();

    glk_set_window(wins[curwin].win);
    /* Print prompt */
    hugo_print(prompt);

    // Asking for line input usually means that we are not in a menu
    // But there is an exception in Haunting the Ghosts.
    if (wins[curwin].win->type != wintype_TextGrid)
    {
        inmenu = false;
//        LOG("Detected that we are NOT in a menu!\n");
    }

    /* Request line input */
    if (wins[curwin].win->char_request)
        glk_cancel_char_event(wins[curwin].win);

    glk_request_line_event(wins[curwin].win, buffer, MAXBUFFER, 0);

    while (ev.type != evtype_LineInput)
    {
        glk_select(&ev);
        if (ev.type == evtype_Arrange)
            hugo_handlearrange();
    }

    /* The line we have received in commandbuf is not null-terminated */
    buffer[ev.val1] = '\0';    /* i.e., the length */

    if (wins[curwin].win->type == wintype_TextGrid)
        win_moveto(wins[curwin].win->peer, 0, currentline + 2);

    /* Copy the input to the script file (if open) */
    if (script)
    {
        glk_put_string_stream(script, prompt);
        glk_put_string_stream(script, buffer);
        glk_put_string_stream(script, "\n");
    }
}


/* hugo_waitforkey
 *
 * Provided to be replaced by multitasking systems where cycling while
 * waiting for a keystroke may not be such a hot idea.
 */

int hugo_waitforkey(void)
{
    event_t ev = {0};

    window_t *win;

    LOG("hugo_waitforKey\n");

    if (keypress)
    {
        int oldkeypress = keypress;
        keypress = 0;
        return oldkeypress;
    }

    /* Ignore calls where there's been no recent display. */
    if (!just_displayed_something && !inmenu)
        return 0;

    just_displayed_something = false;

    int event_requested = 0;

//    hugo_mapcurwin();

    win = wins[curwin].win;
    if (win) {
             if (win->type == wintype_Graphics || win->type == wintype_TextGrid || win->type == wintype_TextBuffer) {
            if (!win->char_request) {
                glk_request_char_event(win);
                LOG("requested char event in win %d\n", win->peer);
            }
            event_requested = 1;
        }

        if (!menuwin && !inmenu)
        {
            if (win->type == wintype_Graphics || win->type == wintype_TextGrid)
            {
                if (!win->mouse_request)
                {
                    glk_request_mouse_event(win);
                    LOG("requested mouse event in win %d\n", win->peer);
                    wins[curwin].clear = true;
                }
            }
            if (win->type == wintype_TextBuffer)
            {
                if (!win->hyper_request)
                {
                    glk_request_hyperlink_event(win);
                    LOG("requested hyperlink event in win %d\n", win->peer);
                }
            }
        }
    }


    if (!event_requested)
    {
        if (!curwin)
            return 0;
        hugo_mapcurwin();
        glk_request_char_event(wins[curwin].win);
    }

    if (wins[curwin].win && wins[curwin].win->type == wintype_TextGrid && (menuwin || inmenu) && !wins[curwin].isaux &&
        !gli_enable_styles)
    {
        glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);
        glk_put_char('*');
    }

    while (ev.type != evtype_CharInput && ev.type != evtype_MouseInput && ev.type != evtype_Hyperlink)
    {
        glk_select(&ev);
        if (ev.type == evtype_Arrange)
            hugo_handlearrange();
    }

    if (ev.type == evtype_MouseInput) {
        // It's a mouse click.
        LOG("hugo_waitforKey: received a mouse input\n");
        display_pointer_x = ev.val1;
        display_pointer_y = ev.val2;
        if (wins[curwin].win && wins[curwin].win->type == wintype_Graphics)
        {
            display_pointer_x = display_pointer_x / gcellw;
            display_pointer_y = display_pointer_y / gcellh;
        }
        LOG("mouse input x = %d mouse input y = %d \n", display_pointer_x, display_pointer_y );
        return 1;
    }

    if (ev.type == evtype_CharInput)
        LOG("hugo_waitforKey: received a key input (%d) for window %d\n", ev.val1, ev.win->peer);

    display_pointer_x = currentpos;
    display_pointer_y = currentline;

    for (win = glk_window_iterate(NULL, NULL); win; win = glk_window_iterate(win, NULL))
    {
        if (win->mouse_request)
        {
            glk_cancel_mouse_event(win);
        }
        if (win->char_request)
        {
            glk_cancel_char_event(win);
        }
        if (win->hyper_request)
        {
            glk_cancel_hyperlink_event(win);
        }
    }

    /* Convert Glk special keycodes: */
    return hugo_convert_key (ev.val1);
}


/* Clear everything on the screen,
 * move the cursor to the top-left
 * corner of the screen
 */

void hugo_clearfullscreen(void)
{
    int i;

    LOG("hugo_clearfullscreen\n");

    nlbufwin = 0;
    nlbufcnt = 0;

    for (i = 1; i < nwins; i++)
    {
        //LOG(" + delete %d\n", i);
        if (wins[i].win)
        {
            gli_delete_window(wins[i].win);
        }
    }

    nwins = 1;
    curwin = 0;
    mainwin = 0;
    statuswin = 0;
    auxwin = 0;
    below_status = 0;
    second_image_row = 0;

    /* create initial fullscreen window */
    hugo_settextwindow(1, 1,
                       SCREENWIDTH,
                       SCREENHEIGHT);

    win_setbgnd(-1, hugo_color(bgcolor));

    currentpos = 0;
    currentline = 1;
}

// Returns true if the two window contexts overlap
int overlap(struct winctx a, struct winctx b)
{
    // If one rectangle is on left side of other
    if (a.x0 >= b.x1 || b.x0 >= a.x1)
        return false;

    // If one rectangle is above other
    if (a.y1 <= b.y0 || b.y1 <= a.y0)
        return false;

    return true;
}

// Returns true if the window context covers the screen
int isfullscreen(struct winctx ctx)
{
    return ((ctx.x0 == 0 && ctx.y0 == 0 && ctx.x0 == gscreenw && ctx.y0 == gscreenh)
         || (ctx.l == 1 && ctx.r == SCREENWIDTH && ctx.t == 1 && ctx.b == SCREENHEIGHT));
}

void checkfutureboyoverlap(int left, int top, int right, int bottom)
{
    int i, quarter, quadrant, req_quadrant;
    // Bail if requested window is lower than main window
    if ((mainwin && top >= wins[mainwin].t) || !isfutureboy)
        return;

    int width = right - left;
    int height = bottom - top;
    // First, we check if this is the large main image
    if (((width > 30 && width < 50 && height > 9 && height < 15) || (screenwidth_in_chars < 60 && ABS(screenwidth_in_chars / 2 - width) < 3))) {
        if (wins[future_boy_main_image].wasmoved && ((left > screenwidth_in_chars / 2 && future_boy_main_image_right) ||
            (left <= screenwidth_in_chars / 2 && !future_boy_main_image_right)))
            wins[future_boy_main_image].clear = 1;
            return;
    }

    quarter = screenwidth_in_chars / 4;
    if (future_boy_main_image_right)
        req_quadrant = left / quarter;
    else
        req_quadrant = right / quarter;

    for (i = 1; i < nwins; i++)
    {
        quarter = wins[i].screenwidth_at_creation / 4;

        if (future_boy_main_image_right)
            quadrant = wins[i].l / quarter;
        else
            quadrant = wins[i].r / quarter;

        if (wins[i].wasmoved && wins[i].win && wins[i].win->type == wintype_Graphics && req_quadrant == quadrant && ABS(wins[i].r - wins[i].l) <= quarter && wins[i].t < wins[mainwin].t) {
            wins[i].clear = 1;
            return;
        }
    }

    return;
}

/* Clear the currently defined window, move the cursor to the top-left
 * corner of the window.
 */

void hugo_clearwindow(void)
{
    if (menuwin) {
        return;
    }

    if (curwin == menuwin)
    {
        currentpos = 0;
        currentline = 1;
        return;
    }

    int l = wins[curwin].l;
    int t = wins[curwin].t;
    int r = wins[curwin].r;
    int b = wins[curwin].b;
    int i;

    LOG("hugo_clearwindow %d: l:%d t:%d r:%d b:%d\n",curwin, l, t, r, b);
//    LOG("hugo_clearwindow bg = %d (0x%x, %d)\n", wins[curwin].bg, hugo_color(wins[curwin].bg), hugo_color(wins[curwin].bg));

    if (curwin == nlbufwin)
    {
        nlbufwin = 0;
        nlbufcnt = 0;
    }

    /* We delete the main window here in order to be able */
    /* to change input text color */
    if (curwin == mainwin && wins[curwin].win && !isfutureboy)
    {
        gli_delete_window(wins[curwin].win);
        wins[curwin].win = 0;
    }

    for (i = 1; i < nwins; i++)
    {
        if (wins[i].l >= l && wins[i].t >= t && wins[i].r <= r && wins[i].b <= b)
        {
            if (wins[i].win)
            {
                if (wins[curwin].win && hugo_color(wins[curwin].bg) != -1)
                {
                    wins[i].bg = wins[curwin].bg;
                    wins[i].papercolor = wins[curwin].bg;
                    win_setbgnd(wins[i].win->peer, hugo_color(wins[i].bg));
                    lastbg = -1;
                }
                LOG("   cleared %d\n", i);
                glk_window_clear(wins[i].win);

                if (i == menuwin)
                {
                    menuwin = 0;
                    menutop = 0;
                    below_status = 0;
                    gli_delete_window(wins[i].win);
                    wins[i].win = NULL;
                    curwin = statuswin;
                    if (wins[statuswin].win)
                    {
                        glk_set_window(wins[statuswin].win);
                    }

                }

                if (i == below_status)
                {
                    below_status = 0;
                    if (mainwin)
                    {
                        below_status = heglk_find_attached_at_top(wins[mainwin].t);
                        if (below_status)
                        {
                            if (below_status == statuswin || wins[below_status].clear)
                            {
                                below_status = 0;
                            }
                        }
                    }
                }
                if (i == second_image_row)
                {
                    second_image_row = 0;
                }
            } else {
                LOG("   win %d has no Glk counterpart (yet), so won't actually be cleared or colored.\n", i);
            }

            wins[i].clear = 1;
            wins[i].curx = 0;
            wins[i].cury = 0;
            wins[i].isaux = 0;
        }
    }

    if (wins[curwin].win)
    {
        win_setbgnd(wins[curwin].win->peer, hugo_color(wins[curwin].bg));
        wins[curwin].papercolor = wins[curwin].bg;
        lastbg = -1;
    }

    // Hack to keep black room description window from
    // being deleted at the first turn of Guilty Bastards
    if (isguiltybastards && wins[curwin].l >= screenwidth_in_chars / 2 && wins[curwin].halfscreenwidth)
    {
        wins[curwin].clear = 0;
    }

    if (isfullscreen(wins[curwin]))
    {
        win_setbgnd(-1, hugo_color(wins[curwin].bg));
        screen_bg = wins[curwin].bg;
        LOG("Window %d filled the screen, so we set the entire window to its background color.\n", curwin);
    }

    currentpos = 0;
    currentline = 1;
}

static void hugo_unmapcleared(void)
{
    int i;

    for (i = 1; i < nwins; i++)
    {
        if (i != curwin && wins[i].clear && wins[i].win)
        {
            LOG(" + unmap window %d\n", i);
            gli_delete_window(wins[i].win);
            wins[i].win = NULL;

            if (i == statuswin)
                statuswin = 0;
            if (i == below_status)
                below_status = 0;
            if (i == second_image_row)
                second_image_row = 0;
            if (i == guilty_bastards_aux_win)
                guilty_bastards_aux_win = 0;
            if (i == guilty_bastards_graphics_win)
                guilty_bastards_graphics_win = 0;

            if (i == nwins - 1)
            {
                nwins--;
            }
        }
    }
}

void hugo_handlearrange(void)
{
    if (heglk_get_linelength() == screenwidth_in_chars && heglk_get_screenheight() == screenheight_in_chars && gcellw * screenwidth_in_chars + ggridmarginx * 2 == gscreenw) {
        return;
    }

    screenwidth_in_chars = heglk_get_linelength();
    screenheight_in_chars = heglk_get_screenheight();

    if (menuwin)
    {
        in_arrange_event = true;
        heglk_ensure_menu();
        in_arrange_event = false;
    }

    if (statuswin && wins[statuswin].win)
    {
        if (!menuwin)
        {
            wins[statuswin].y1 = ggridmarginy * 2 + wins[statuswin].b * gcellh;
        }
        wins[statuswin].x1 = gscreenw;
        win_sizewin(wins[statuswin].win->peer, wins[statuswin].x0, wins[statuswin].y0, wins[statuswin].x1, wins[statuswin].y1);
        if (below_status)
        {
            wins[below_status].y0 = wins[statuswin].y1;
            if (mainwin && below_status != mainwin && !second_image_row)
                wins[below_status].y1 = heglk_push_down_main_window_to(wins[below_status].y1);

            for (int i = 1; i < nwins; i++)
            {
                int oldx0 = wins[i].x0;
                int oldy0 = wins[i].y0;
                int oldx1 = wins[i].x1;
                int oldy1 = wins[i].y1;

                if (wins[i].halfscreenwidth && !isfutureboy)
                {
                    if (isguiltybastards && i != statuswin)
                    {
                        below_status = i;
                    } else {
                        if (wins[i].peggedtoright)
                        {
                            wins[i].x0 = gscreenw / 2;
                        } else {
                            wins[i].x1 = wins[i].x0 + gscreenw / 2;
                        }
                    }
                }
                if (isczk && wins[i].isaux)
                {
                    int toleft = heglk_find_attached_to_left(wins[i].l);
                    if (toleft)
                        wins[i].x0 = wins[toleft].x1;
                    wins[i].y1 = wins[mainwin].y0;
                    if (wins[i].win && wins[i].win->type == wintype_TextBuffer)
                        wins[i].x1 = gscreenw;
                }

                if (wins[i].x0 != oldx0 || wins[i].y0 != oldy0 || wins[i].x1 != oldx1 || wins[i].y1 != oldy1)
                {
                    heglk_sizeifexists(i);
                }
                wins[i].wasmoved = true;
            }

        }
    }

    if (mainwin && wins[mainwin].win)
    {
        wins[mainwin].x1 = gscreenw;
        wins[mainwin].x0 = 0;
        wins[mainwin].y1 = gscreenh;
        win_sizewin(wins[mainwin].win->peer, wins[mainwin].x0, wins[mainwin].y0, wins[mainwin].x1, wins[mainwin].y1);
    }

    heglk_adjust_guilty_bastards_windows();
}

void heglk_sizeifexists(int i)
{
    if (wins[i].win)
    {
        if ( wins[i].x0 < 0)
        {
            wins[i].x0 = 0;
        }
        if (wins[i].y0 > wins[i].y1)
        {
            int temp = wins[i].y0;
            wins[i].y0 = wins[i].y1;
            wins[i].y1 = temp;
        }

        if (wins[i].x0 > wins[i].x1)
        {
            int temp = wins[i].x0;
            wins[i].x0 = wins[i].x1;
            wins[i].x1 = temp;
        }

        if (wins[i].y0 == wins[i].y1)
        {
            LOG("glk_sizeifexists: Window %d was resized to height 0!\n", i);
        }
        win_sizewin(wins[i].win->peer, wins[i].x0, wins[i].y0, wins[i].x1, wins[i].y1);
        LOG("glk_sizeifexists: Resized window %d to %d %d %d %d\n", i, wins[i].x0, wins[i].y0, wins[i].x1, wins[i].y1);
    } else {
        LOG("glk_sizeifexists: Window %d has no Glk counterpart!\n", i);
    }
}

/* Pre-v2.4 versions had no proper concept of windows, */
/* so we guess the status window height here and in */
/* in heglk_adjust_pre_v24_status_when_printing() below */

int heglk_adjust_pre_24_status_when_setting_window(int bottom)
{
    if (game_version >= 24 || bottom <= screenheight_in_chars)
        return bottom;

    if (statuswin && wins[statuswin].clear == 0 && wins[statuswin].b < bottom)
    {
        bottom = wins[statuswin].b;
        if (lowest_printed_position_in_status < bottom)
            bottom = lowest_printed_position_in_status;
    } else {
        bottom = 1;
    }
    return bottom;
}

int heglk_push_down_main_window_to(int y)
{
    LOG("heglk_push_down_main_window_to %d\n", y);
    if (!mainwin || !y)
    {
        return y;
    }

    if (statuswin && wins[statuswin].y1)
        y = MAX(y, wins[statuswin].y1);

    if (second_image_row && wins[second_image_row].y1)
        y = MIN(y, wins[second_image_row].y1);

    if (isguiltybastards && y - wins[mainwin].y0 > gcellh * 10)
        return wins[mainwin].y0;

    int minimal_mainwin_height = gbuffermarginy * 2 + gbufcellh * 3;
    if (wins[mainwin].win && wins[mainwin].win->type == wintype_TextGrid)
        minimal_mainwin_height = ggridmarginy * 2 + gcellh * 3;
    if (gscreenh <  minimal_mainwin_height)
        minimal_mainwin_height = gscreenh / 2;
    if (gscreenh - y < minimal_mainwin_height)
        y = gscreenh - minimal_mainwin_height;
    wins[mainwin].y0 = y;
    heglk_sizeifexists(mainwin);
    LOG("Pushed down main window to %d\n", y);
    if (!y) {
        LOG("y == 0!\n");
    }
    return y;
}

int heglk_find_attached_at_top(int top)
{
    for (int i = 1; i < nwins; i++)
    {
        if (ABS(wins[i].b - top) <= 1) {
            LOG("Window %d (l:%d, t:%d, r:%d, b:%d) is attached to top\n", i, wins[i].l, wins[i].t, wins[i].r, wins[i].b);
            return i;
        }
    }
    return 0;
}

int heglk_find_attached_to_left(int left)
{
    for (int i = 1; i < nwins; i++)
    {
        if (ABS(wins[i].r - left) <= 1)
        {
            LOG("Window %d (l:%d, t:%d, r:%d, b:%d) is attached to the left\n", i, wins[i].l, wins[i].t, wins[i].r, wins[i].b);
            return i;
        }
    }
    return 0;
}

void heglk_ensure_menu(void)
{
    LOG("heglk_ensure_menu\n");
    LOG("Splitting status window into status + menu\n");

    // We only want two windows, so delete all others
    for (int i = 1; i < nwins; i++) {
        if (i != menuwin && i != statuswin)
        {
            wins[i].clear = 1;
            if (wins[i].win)
            {
                gli_delete_window(wins[i].win);
                wins[i].win = NULL;
            }
        }
    }

    nwins = MAX(menuwin, statuswin) + 1;
    mainwin = menuwin;

    guilty_bastards_aux_win = 0;
    guilty_bastards_graphics_win = 0;

    if (!menuwin)
    {
        if (statuswin == 1)
        {
            menuwin = 2;
            nwins = 3;
        }
        else
        {
            menuwin = 1;
            nwins = statuswin + 1;
        }

        if (!wins[menuwin].win )
        {
            wins[menuwin].win = gli_new_window(wintype_TextGrid, 0);
        }

        if (!menutop)
        {
            menutop = currentline;
            LOG("menu top:%d\n", menutop);
        }
    }

    LOG("menuwin id:%d statuswin:%d nwins:%d\n", menuwin, statuswin, nwins);

    wins[menuwin].t = menutop;
    wins[menuwin].l = 1;
    wins[menuwin].b = screenheight_in_chars;
    wins[menuwin].r = screenwidth_in_chars;
    wins[menuwin].x0 = 0;
    wins[menuwin].x1 = gscreenw;
    below_status = menuwin;
    second_image_row = 0;
    curwin = menuwin;

    wins[menuwin].y0 = (menutop - 1 - (menutop > 2)) * gcellh + 2 * ggridmarginy;
    wins[menuwin].y1 = gscreenh;

    if (statuswin == menuwin)
    {
        statuswin = 0;
    }
    if (statuswin)
    {
        wins[menuwin].fg = wins[statuswin].fg;
        wins[menuwin].bg = wins[statuswin].bg;
        win_setbgnd(wins[menuwin].win->peer, hugo_color(wins[menuwin].bg));
        wins[menuwin].papercolor = wins[menuwin].bg;
        lastbg = -1;
    } else {
        statuswin = nwins++;
    }
    wins[statuswin].y1 = wins[menuwin].y0;

    if (!wins[statuswin].win)
        wins[statuswin].win = gli_new_window(wintype_TextGrid, 0);

    wins[menuwin].clear = 0;
    wins[statuswin].clear = 0;

    heglk_sizeifexists(statuswin);
    heglk_sizeifexists(menuwin);
    glk_set_window(wins[menuwin].win);

    // Hack to remove leftover lines at menu bottom
    // in Clockwork Boy 2 and The Next Day
    if (!in_arrange_event && (iscwb2 || isnextday))
    {
        glk_window_clear(wins[menuwin].win);
    }
    glk_window_move_cursor(wins[menuwin].win, 0, 0);
}

void heglk_adjust_guilty_bastards_windows(void)
{
    if (!(isguiltybastards && guilty_bastards_graphics_win && guilty_bastards_aux_win && statuswin && mainwin))
        return;

    for (int i = 1; i < nwins; i ++)
    {
        if (wins[i].win && wins[i].win->type == wintype_Graphics && i != guilty_bastards_graphics_win)
        {
            gli_delete_window(wins[i].win);
            wins[i].win = NULL;
            wins[i].clear = 1;
        }
    }

    int diff = wins[statuswin].y1 - wins[guilty_bastards_graphics_win].y0;

    wins[guilty_bastards_graphics_win].y0 = wins[statuswin].y1;
    wins[guilty_bastards_graphics_win].y1 += diff;

    if (gscreenw < wins[guilty_bastards_graphics_win].x1 || gscreenw > wins[guilty_bastards_graphics_win].x1 * 3)
        wins[guilty_bastards_graphics_win].y1 = gscreenw / 2;

    if (gscreenh < wins[guilty_bastards_graphics_win].y1 + wins[guilty_bastards_graphics_win].y0)
        wins[guilty_bastards_graphics_win].y1 = gscreenh / 2;

    wins[guilty_bastards_aux_win].y0 = wins[guilty_bastards_graphics_win].y0;
    wins[guilty_bastards_aux_win].y1 = wins[guilty_bastards_graphics_win].y1;
    wins[guilty_bastards_aux_win].x0 = wins[guilty_bastards_graphics_win].x1 - 1;
    wins[guilty_bastards_aux_win].x1 = gscreenw;

    /* For some reason this is called when on our custom menu screen,
     so sanity check before pushing down main window */
    if (wins[mainwin].t > 3)
    {
        wins[mainwin].y0 = wins[guilty_bastards_graphics_win].y1;
        heglk_sizeifexists(mainwin);
    }

    heglk_sizeifexists(guilty_bastards_graphics_win);
    heglk_sizeifexists(guilty_bastards_aux_win);
}

void hugo_settextwindow(int left, int top, int right, int bottom)
{
    int x0, y0, x1, y1;
    int type = ISMAIN;
    int new_below_status = false;
    int new_second_image_row = false;
    int matches = 0;
    int halfscreenwidth = false;
    int peggedtoright = false;

    int i;

    if (curwin >= 0)
    {
        wins[curwin].cury = currentline;    /* one-based */
        wins[curwin].curx = currentpos;        /* zero-based */
    }

    LOG("hugo_settextwindow %d %d %d %d\n", left, top, right, bottom);

    if (bottom < top)
    {
        LOG("hugo_settextwindow: negative height. Bailing.\n");
        return;
    }

    if (menuwin)
    {
        LOG("hugo_settextwindow: We are in a menu\n");
        if (left == 1 && (top == 1 || top > 5) && right == SCREENWIDTH && (bottom == SCREENHEIGHT || bottom > 3))
        {
            if (bottom == SCREENHEIGHT || top > 5) {
                LOG("hugo_settextwindow: Just setting window background, bailing, doing nothing\n");
                return;
            }
            heglk_ensure_menu();
            return;
        } else {
            LOG("hugo_settextwindow: Guessing that we are trying to leave the menu. Clearing menu window and carrying on.\n");
            wins[menuwin].clear = 1;
            curwin = menuwin;
            mainwin = menuwin;
            menuwin = 0;
        }
    }

    /* find a match */
    curwin = 0;
    for (i = 1; i < nwins; i++)
    {
        if
            (wins[i].l == left && wins[i].t == top &&
             wins[i].r == right && wins[i].b == bottom)
        {
            curwin = i;
//            LOG("hugo_settextwindow: found a match! (%d). main = %d, status = %d\n", i, mainwin, statuswin);
            matches++;
        }
    }

    if (matches > 1)
    {
        LOG("hugo_settextwindow: Found more than one match! (%d)\n", matches);
    }

    if (isfutureboy  && !curwin)
        checkfutureboyoverlap(left, top, right, bottom);

    /* unmap old unused windows that are cleared */
    hugo_unmapcleared();

    if ((top != 1 || bottom >= physical_windowbottom/FIXEDLINEHEIGHT+1)
        /* Pre-v2.4 didn't support proper windowing */
        && (game_version >= 24 || !inwindow))
    {
        LOG("Detected that we are setting an aux window. Or is it the main window?\n");

        in_valid_window = false;

        /* Glk-illegal floating window; setting currentwin
         to NULL will tell hugo_print() not to print in it:
         */
        if (bottom < physical_windowbottom/FIXEDLINEHEIGHT+1 || (isfutureboy && bottom < INFINITE))
        {
            type = ISAUX;
        }
        else {
            LOG("It is the main window, actually.\n");
            // type ISMAIN is the default
            if (left == 1 && top == 1 && right >= INFINITE && bottom >= INFINITE)
            {
                if (below_status && below_status != mainwin)
                {
                    below_status = mainwin;
                }
            }
        }
    } else {
        /* Otherwise this is a valid window (positioned along the
         top of the screen a la a status window), so... */

        LOG("Detected that we are setting a status window\n");
        type = ISSTATUS;

        if (isfutureboy && bottom == 26)
            type = ISAUX;
    }

    x0 = 0;
    y0 = 0;
    x1 = gscreenw;
    y1 = bottom * gcellh + 2 * ggridmarginy;

    switch (type) {
        case ISSTATUS:
            bottom = heglk_adjust_pre_24_status_when_setting_window(bottom);
            y1 = bottom * gcellh + 2 * ggridmarginy;
            if (!below_status && mainwin)
                below_status = mainwin;
            if (below_status)
            {
                wins[below_status].y0 = y1;
                if (wins[below_status].y1 < y1)
                    wins[below_status].y1 = y1;
                if (below_status == mainwin)
                {
                    wins[below_status].t = bottom + 1;
                }
                heglk_sizeifexists(below_status);
            }
            if (curwin)
            {
                if (curwin == mainwin)
                {
                    curwin = 0;
                }
            }
            if (statuswin)
            {
                curwin = statuswin;
                if (wins[statuswin].win)
                    glk_window_clear(wins[statuswin].win);
            }
            right = screenwidth_in_chars + 2;
            break;
        case ISAUX:
            if (curwin)
            {
                if (!wins[curwin].isaux)
                {
                    LOG("Setting window detected as aux, but reused window is not aux!?\n");
                    if (curwin == mainwin)
                    {
                        LOG("  It was actually mainwin\n");
                    }
                    if (curwin == statuswin)
                    {
                        LOG("  It was actually statuswin\n");
                    }
                    wins[curwin].isaux = true;
                }
            }

            x0 = (left - 1) * gcellw;
            y0 = (top - 1) * gcellh; //+ 2 * ggridmarginy * (top > 1);
            x1 = right * gcellw;

            if (gscreenw - x1 < 3 * gcellw || right > screenwidth_in_chars - 1)
                x1 = gscreenw;

            if (ABS((right - left) - (screenwidth_in_chars / 2)) < 2) //&& !isfutureboy)
            {
                x1 = x0 + gscreenw / 2;
                halfscreenwidth = true;

                LOG("Detected that aux window is supposed to be half screen width. Stretching it out.\n");

                // Hack to keep black room description window from
                // being deleted at the first turn of Guilty Bastards
                if (isguiltybastards && curwin)
                {
                    if (left == 1)
                    {
                        guilty_bastards_graphics_win = curwin;
                    }
                    else
                    {
                        guilty_bastards_aux_win = curwin;
                        hugo_mapcurwin();
                        wins[curwin].clear = 0;
                        x0 = gscreenw / 2;
                        x1 = gscreenw;
                        peggedtoright = 1;
                    }
                }
            }

            if (ABS(right - screenwidth_in_chars) < 2 && !heglk_find_attached_to_left(left))
            {
                int diff = gscreenw - x1;
                x1 = gscreenw;
                x0 += diff;
                peggedtoright = true;
            }

            y1 = y0 + gcellh * (bottom - top + 1);

//            LOG("statuswin = %d wins[statuswin].b = %d top = %d ABS(top - wins[statuswin].b) = %d\n", statuswin, wins[statuswin].b, top, ABS(top - wins[statuswin].t));

            if (statuswin && ABS(top - wins[statuswin].b) < 2)
            {
                new_below_status = true;
            } else if (!statuswin && top <= 2) {
                new_below_status = true;
                int diff = 2 * ggridmarginy + gcellh + gcellh * (screenwidth_in_chars < MAX_STATUS_CHARS) - y0;
                y0 += diff;
                y1 += diff;
            } else {
                int attop = heglk_find_attached_at_top(top);
                if (!second_image_row && attop && (attop == below_status || wins[attop].t == wins[below_status].t))
                {
                    new_second_image_row = true;
                }
            }
            break;
        case ISMAIN:
            if (curwin)
            {
                if (curwin == statuswin)
                {
                    curwin = 0;
                }
            }
            if (mainwin)
            {
                curwin = mainwin;
            }
            y1 = gscreenh;
            if (statuswin && !wins[statuswin].clear && (below_status || second_image_row))
            {
                if (top == 1 && below_status && below_status != mainwin)
                {
                        wins[below_status].clear = 1;
                        below_status = mainwin;
                }
                int above_main = heglk_find_attached_at_top(top);

                if (!above_main) {
                    above_main = heglk_find_attached_at_top(wins[mainwin].t);
                    if (!above_main) {
                        if (second_image_row)
                        {
                            above_main = second_image_row;
                        } else if (statuswin)
                        {
                            above_main = statuswin;
                        }
                    }
                }

                if (above_main && above_main != mainwin)
                {
                    y0 = heglk_push_down_main_window_to(wins[above_main].y1);
//                    LOG("hugo_settextwindow: Adjusted main window top from %d to %d (%d) (bottom of window %d)\n", top, wins[above_main].b,  wins[above_main].y1, above_main);
                } else {
                    y0 = heglk_push_down_main_window_to(wins[statuswin].y1);
//                    LOG("hugo_settextwindow: Adjusted main window top from %d to %d (%d) (bottom of status window)\n", top, wins[statuswin].b,  wins[statuswin].y1);
                }
            }

            if (!statuswin && !below_status && !second_image_row)
            {
                new_below_status = true;
            }
            break;
        default:
            LOG("Unhandled switch case!\n");
            break;
    }

    if (!curwin)
    {
        /* ... create a new window context if no suitable exists... */
        curwin = nwins++;
        LOG("Created new window context %d\n", curwin);
        wins[curwin].win = NULL;
        wins[curwin].clear = 1;
        wins[curwin].fg = DEF_FCOLOR;
        wins[curwin].wasmoved = false;

        wins[curwin].screenwidth_at_creation = screenwidth_in_chars;

        if (screen_bg != -1)
            wins[curwin].bg = screen_bg;
        else
            wins[curwin].bg = DEF_BGCOLOR;

        if (type == ISMAIN)
        {
            mainwin = curwin;
        } else if (type == ISSTATUS)
        {
            statuswin = curwin;
            wins[curwin].fg = DEF_SLFCOLOR;
            if (screen_bg != -1)
            {
                wins[curwin].bg = screen_bg;
            } else {
                wins[curwin].bg = DEF_SLBGCOLOR;
            }
        }
        wins[curwin].papercolor = wins[curwin].bg;
        lastbg = -1;
    } else {
        LOG("Reused window context %d\n", curwin);
    }

    wins[curwin].isaux = (type == ISAUX);
    wins[curwin].peggedtoright = peggedtoright;
    wins[curwin].halfscreenwidth = halfscreenwidth;

    if (new_below_status)
    {
        below_status = curwin;
        if (statuswin)
        {
            y0 = wins[statuswin].y1;
        }
//        LOG("Attached %s window %d below status\n", (curwin == mainwin) ? "main" : "aux", curwin);
    }

    if (new_second_image_row)
    {
        second_image_row = curwin;
        y1 = heglk_push_down_main_window_to(y1);
    }

    if (y1 > gscreenh)
        y1 = gscreenh;
    if (x1 > gscreenw)
        x1 = gscreenw;

//    int changed = false;
//    if (reused)
//    {
//        LOG("hugo_settextwindow: Old values of ctx %d: l:%d t:%d r:%d b:%d, x0:%d y0:%d x1:%d y1:%d\n", curwin, wins[curwin].l, wins[curwin].t, wins[curwin].r, wins[curwin].b, wins[curwin].x0, wins[curwin].y0, wins[curwin].x1, wins[curwin].y1);
//        if (wins[curwin].l != left) {
//            LOG("wins[curwin].l was changed from %d to %d\n", wins[curwin].l, left);
//            changed = true;
//        }
//        if (wins[curwin].t != top) {
//            LOG("wins[curwin].t was changed from %d to %d\n", wins[curwin].t, top);
//            changed = true;
//        }
//        if (wins[curwin].r != right) {
//            LOG("wins[curwin].r was changed from %d to %d\n", wins[curwin].r, right);
//            changed = true;
//        }
//        if (wins[curwin].b != bottom) {
//            LOG("wins[curwin].r was changed from %d to %d\n", wins[curwin].b, bottom);
//            changed = true;
//        }
//        if (wins[curwin].x0 != x0) {
//            LOG("wins[curwin].x0 was changed from %d to %d\n", wins[curwin].x0, x0);
//            changed = true;
//        }
//        if (wins[curwin].y0 != y0) {
//            LOG("wins[curwin].y0 was changed from %d to %d\n", wins[curwin].y0, y0);
//            changed = true;
//        }
//        if (wins[curwin].x1 != x1) {
//            LOG("wins[curwin].x1 was changed from %d to %d\n", wins[curwin].x1, x1);
//            changed = true;
//        }
//        if (wins[curwin].y1 != y1) {
//            LOG("wins[curwin].y1 was changed from %d to %d\n", wins[curwin].y1, y1);
//            changed = true;
//        }
//        if (!changed) {
//            LOG("hugo_settextwindow: No values were changed in reused context!\n");
//        }
//    }
    // If we reuse the context,
    // most of these should will have the right value already...
    wins[curwin].l = left;
    wins[curwin].t = top;
    wins[curwin].r = right;
    wins[curwin].b = bottom;
    wins[curwin].x0 = x0;
    wins[curwin].y0 = y0;
    wins[curwin].x1 = x1;
    wins[curwin].y1 = y1;

//    LOG("hugo_settextwindow (%d): l:%d t:%d r:%d b:%d was translated to x0:%d y0:%d x1:%d y1:%d\n", curwin, origleft, origtop, origright, origbottom, x0, y0, x1, y1);

    heglk_sizeifexists(curwin);

    heglk_adjust_guilty_bastards_windows();

    heglk_record_physical(wins[curwin]);
}

void hugo_settextpos(int x, int y)
{
//    LOG("hugo_settextpos %d %d. Currentline:%d currentpos:%d\n", x, y, currentline, currentpos);

    /* The top-left corner of the current active window is (1, 1). */

    currentline = y;

    if (y == 2 && wins[curwin].clear)
        currentline = 1;

    currentpos = (x-1) * CHARWIDTH;   /* Note:  zero-based */

    if (wins[curwin].win == NULL)
    {
        return;
    }

    if (y >= INFINITE)
    {
        if ((wins[curwin].win->type == wintype_TextGrid && !menuwin) || wins[curwin].win->type == wintype_Graphics) {
            LOG("  moved to line %d in grid window. unmap.\n", y);
            if (isfutureboy && wins[curwin].win->type == wintype_Graphics) {
                gli_delete_window(wins[curwin].win);
                wins[curwin].win = NULL;
                hugo_mapcurwin();
                return;
            }
            gli_delete_window(wins[curwin].win);
            wins[curwin].win = NULL;
        }
        return;
    }
    else if (wins[curwin].win->type == wintype_TextBuffer)
    {
        // Hack to keep black room description window from
        // being deleted at the first turn of Guilty Bastards
        if (isguiltybastards && wins[curwin].l >= screenwidth_in_chars / 2 && wins[curwin].halfscreenwidth)
        {
            LOG(" move in buffer window, Guilty bastards. x:%d y:%d\n", x, y);
            return;
        }
        if (showing_futureboy_title)
        {
            LOG(" move in buffer window, showing Future Boy! title\n");
            return;
        }

        if (showing_author_photo)
        {
            int y1 = 0;
            for (int i = 0; i < nwins; i++) {
                if (!wins[i].clear && wins[i].win && wins[i].win->type == wintype_Graphics) {
                    y1 = wins[i].y1;
                    break;
                }
            }
            wins[mainwin].y0 = y1 - gcellh;
            heglk_sizeifexists(mainwin);
            showing_author_photo = 0;
            return;
        }

        LOG("  move in buffer window. unmap.\n");
        if (!wins[curwin].clear && !wins[curwin].isaux && !isfutureboy)
            hugo_waitforkey();
        gli_delete_window(wins[curwin].win);
        wins[curwin].win = NULL;
    }
    else if (wins[curwin].win->type == wintype_TextGrid)
    {
        if (menuwin)
        {
            LOG("  hugo_settextpos in menu window.\n");
            if (currentline < menutop)
            {
                LOG("currentline (%d) is less than menutop (%d)  moving to status window.\n", currentline, menutop);
                curwin = statuswin;
                glk_set_window(wins[statuswin].win);

            } else {
                LOG("currentline (%d) is greater than or equal to menutop (%d)  moving to menu window line %d\n", currentline, menutop, currentline - menutop);
                curwin = menuwin;
                glk_set_window(wins[menuwin].win);
                glk_window_move_cursor(wins[curwin].win, currentpos, currentline - menutop);
                return;
            }
        }
        glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);
    }
}

/* Check if the current "context" wins[curwin] has a corresponding Glk Window object. If not, create one */
static void hugo_mapcurwin()
{
    //        LOG("hugo_mapcurwin\n");

    if (!curwin || wins[curwin].win != NULL)
        return; // The current window context already has a corresponding Glk window

    if ((curwin == mainwin
         && (currentline > INFINITE || !mono)) || // We are in a standard, main textbuffer window

        // Hack to give fixed-width font in Haunting the Ghosts menu proper word wrapping
        (ishtg && currentline == 1 && statuswin && curwin != statuswin && wins[curwin].fg == 7 && wins[curwin].bg == 0) ||

        // Hack to give fixed-width font in World Builder menu proper word wrapping
        // (Haunting the Ghosts and World Builder use a fixed-width style for the regular game text,
        // so the usual trick of checking for a fixed-with font doesn't work)
        (isworldbuilder && currentline == 1 && statuswin && curwin != statuswin && !menuwin) ||

        (!mono && inwindow && wins[curwin].l > 5))  // We are in a right-aligned aux textbuffer window
    {
        LOG(" * map win to buffer %d\n", curwin);

        /* We try to change to input style text color to something readable, */
        /* i.e. different from the background color */
        if (wins[curwin].bg != DEF_BGCOLOR && wins[curwin].fg != DEF_FCOLOR)
            glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, hugo_color(wins[curwin].fg));
        else
            glk_stylehint_clear(wintype_TextBuffer, style_Input, stylehint_TextColor);

        if (ismarjorie && wins[curwin].fg == DEF_FCOLOR && gfgcol == 0 && wins[curwin].bg == 0)
        {
            glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, hugo_color(HUGO_WHITE));
        }

        wins[curwin].win = gli_new_window(wintype_TextBuffer, 0);
        if (isczk && !mono && inwindow && wins[curwin].l > 5 && wins[mainwin].y0 > wins[curwin].y1)
        {
                wins[curwin].y1 = wins[mainwin].y0;
        }

    }
    else
    {
        LOG(" * map win to grid %d\n", curwin);

        if (currentline == 1 && statuswin && curwin != statuswin && !menuwin && wins[curwin].b > screenheight_in_chars )
        {
            LOG("wins[curwin].b = %d\n", wins[curwin].b );
            LOG("wins[curwin].t = %d\n", wins[curwin].t );
            LOG("wins[curwin].fg = %d\n", wins[curwin].fg );

            LOG("wins[curwin].bg = %d\n", wins[curwin].bg );
            LOG("statuswin = %d\n", statuswin );
            LOG("wins[statuswin].b = %d\n", wins[statuswin].b );
            LOG("wins[curwin].y1 = %d\n", wins[curwin].y1 );
            LOG("inmenu = %d\n", inmenu );
            LOG("currentpos = %d\n", currentpos );
            LOG("currentline = %d\n", currentline );

        }

        wins[curwin].win = gli_new_window(wintype_TextGrid, 0);

        int quartz = 0;
        if (curwin == statuswin)
        {
            LOG(" - is status window\n");

        } else if (curwin == mainwin)
        {
            LOG(" - is main window\n");
            // If the main window is a grid, we guess that this is a menu
            inmenu = true;
            LOG("Guessing that we are in a menu!\n");
        } else {
            LOG(" - is aux window\n");
        }

        if (curwin == mainwin)
        {
            //LOG(" - ismain, check transparency\n");
            for (int i = 1; i < nwins; i++) {
                if (wins[i].win && wins[i].win->type == wintype_Graphics)
                    quartz = 1;
            }
        }
        else if (istetris) {
            quartz = 1;
        }

        if (quartz)
        {
            win_maketransparent(wins[curwin].win->peer);
            wins[curwin].y0 = wins[curwin].t * gcellh;
            below_status = 0;
            inmenu = 0;
        }
    }

    win_sizewin(wins[curwin].win->peer,
                wins[curwin].x0, wins[curwin].y0,
                wins[curwin].x1, wins[curwin].y1);

    LOG("hugo_mapcurwin %d, bg = %d (0x%x, %d)\n", curwin, wins[curwin].bg, hugo_color(wins[curwin].bg), hugo_color(wins[curwin].bg));
    wins[curwin].papercolor = wins[curwin].bg;
    lastbg = -1;
    if (hugo_color(wins[curwin].bg) != -1) {
        win_setbgnd(wins[curwin].win->peer, hugo_color(wins[curwin].bg));
    }

    if (wins[curwin].win->type == wintype_TextGrid)
    {
        if (inwindow && wins[curwin].l > 2 && wins[curwin].t > 2)
        {
            currentpos = 0;
            currentline = 1;
        }
        glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);

    }
}

void hugo_flushnl(void)
{
    if (nlbufcnt && nlbufwin >= 0)
    {
        glk_set_window(wins[nlbufwin].win);
        if (nlbufcnt > 2)
            nlbufcnt = 2;
        while (nlbufcnt--) {
            glk_put_char('\n');
        }
        nlbufwin = 0;
        nlbufcnt = 0;
        glk_set_window(wins[curwin].win);
    }
}

void hugo_printnl(void)
{
    if (wins[curwin].win->type == wintype_TextBuffer)
    {
        if (nlbufwin != curwin) {
            hugo_flushnl();
        }
        nlbufwin = curwin;
        nlbufcnt ++;
    }
    else
    {
        glk_put_char('\n');
    }
}

/* Pre-v2.4 versions had no proper concept of windows, */
/* so we guess the status window height here and in */
/* in heglk_adjust_pre_v24_status_when_setting_window() below */

void heglk_adjust_pre_v24_status_when_printing(void)
{
    if (game_version >= 24 || curwin != statuswin)
    {
        return;
    }

    if (wins[curwin].clear)
        lowest_printed_position_in_status = 0;

    int actual_height_in_characters = (wins[curwin].y1 - wins[curwin].y0 - 2 * ggridmarginy) / gcellh;
    if ( currentline > actual_height_in_characters && currentline < screenheight_in_chars)
    {
        LOG("Current line %d is below bottom of status line (height %d)\n", currentline, actual_height_in_characters);
        wins[curwin].b += (currentline - actual_height_in_characters);
        if (wins[curwin].b == wins[curwin].t)
            wins[curwin].b++;
        LOG("Expanded it to %d\n", wins[curwin].b - wins[curwin].t);
        wins[curwin].y1 = wins[curwin].y0 + currentline * gcellh + 2 * ggridmarginy;
        if (mainwin) {
            wins[mainwin].t = wins[statuswin].b + 1;
            wins[mainwin].y0 = wins[statuswin].y1;
        }
        heglk_sizeifexists(mainwin);
        heglk_sizeifexists(statuswin);
        glk_window_move_cursor(wins[statuswin].win, currentpos, currentline - 1);
        lowest_printed_position_in_status = currentline - 1 +
        // Hack to make Colossal Cave status bar grow when needed
        (screenwidth_in_chars < MAX_STATUS_CHARS && !inmenu);
    }

    // Hack to make Colossal Cave status bar shrink when needed
    if (lowest_printed_position_in_status > 1 && (screenwidth_in_chars > MAX_STATUS_CHARS || inmenu))
        lowest_printed_position_in_status = 1;
}

void heglk_zcolor(void)
{
    if (!wins[curwin].win)
        return;
    glk_set_window(wins[curwin].win);

    int fg = wins[curwin].fg;
    int bg = wins[curwin].bg;

    glk_fgcolor = hugo_color(fg);
    glk_bgcolor = hugo_color(bg);

    int fg_result = glk_fgcolor;
    int bg_result = glk_bgcolor;

    if (wins[curwin].win->type == wintype_TextBuffer) {
        if (fg_result < 0)
            fg_result = gfgcol;
        if (bg_result < 0)
            bg_result = gbgcol;

        if (bg == DEF_BGCOLOR &&
            ((currentpos == 0 && currentline == 1) || wins[curwin].clear))
        {
            win_setbgnd(wins[curwin].win->peer, -1);
            LOG("Trying to set window background back to default\n");
        }
    }

    if (bg == DEF_SLBGCOLOR && wins[curwin].win->type == wintype_TextGrid) {
        if ((currentpos == 0 && currentline == 1) || wins[curwin].clear)
        {
            win_setbgnd(wins[curwin].win->peer, -1);
            LOG("Trying to set window background back to default\n");
        }
        if (wins[curwin].papercolor != DEF_SLBGCOLOR)
            glk_bgcolor = gsbgcol;
    }

    if (glk_fgcolor == -1 && glk_bgcolor == -1)
    {
        glk_fgcolor = glk_bgcolor = -2;
    }

    if (fg_result == 0 && (bg_result == 0 || (glk_bgcolor < 0 && wins[curwin].papercolor == 0)))
    {
        glk_fgcolor = hugo_color(HUGO_WHITE);
    }

//    LOG("Setting zcolors to %x (%d), %x (%d)\n", glk_fgcolor, glk_fgcolor, glk_bgcolor, glk_bgcolor);
    garglk_set_zcolors(glk_fgcolor, glk_bgcolor) ;
}



void hugo_print(char *a)
{
    /* Essentially the same as printf() without formatting, since printf()
     generally doesn't take into account color setting, font changes,
     windowing, etc.
     */

    if (!curwin)
    {
        LOG("  Tried to print without a window!\n");
        return;
    }

    hugo_unmapcleared();

    /* printing to a window that is showing graphics... unmap */
    if (wins[curwin].win && wins[curwin].win->type == wintype_Graphics)
    {
        if (isfutureboy && (a[0] == ' ' || a[0] == '\r' || a[0] == '\n'))
            return;
        else
            fprintf(stderr, "%d\n", a[0]);
        LOG("  unmap graphics window %d for printing\n", curwin);
        if (!wins[curwin].clear )
            hugo_waitforkey();
        gli_delete_window(wins[curwin].win);
        wins[curwin].win = 0;
    }

    /* check if we are trying to print a menu in the main buffer window */
    // Enceladus
    if (curwin == mainwin && mono && a[0] == '[' && wins[mainwin].clear && !inwindow)
    {
        hugo_settextpos(1, 1);
    }

    hugo_mapcurwin();

    glk_set_window(wins[curwin].win);

    //if (curstyle != laststyle)
    glk_set_style(curstyle);

    laststyle = curstyle;

    if (mono && a[0] == '[')
    {
        inmenu = true;
        if (curwin == statuswin && currentline > 1)
        {
            LOG("We are trying to print a menu\n");
            heglk_ensure_menu();
        }
    }

    heglk_adjust_pre_v24_status_when_printing();

    wins[curwin].clear = 0;

//    LOG("hugo print: '%s' %s %s inwin: %d inmenu %d x:%d y:%d curwin:%d length:%lu\n", a, mono ? "mono" : "prop", (wins[curwin].win->type == wintype_TextGrid) ? "grid" : "buf", inwindow, inmenu, currentpos, currentline, curwin, strlen(a));


    if (a[0] == '\r')
    {
        hugo_printnl();
    }
    else if (a[0] != '\n')
    {
        if (!(wins[curwin].fg == lastfg && wins[curwin].bg == lastbg))
        {
            heglk_zcolor();
        }

        hugo_flushnl();

        lastfg = wins[curwin].fg;
        lastbg = wins[curwin].bg;

        if (istetris && a[0] == 'B' && mono)
        {
            win_moveto(wins[curwin].win->peer, currentpos, currentline - 2);
            glk_put_string("Score:");
            hugo_settextpos(currentpos + 1, currentline);
        }
        glk_put_string(a);

    }

    just_displayed_something = true;
}

void hugo_font(int f)
{
    /* The <f> argument is a mask containing any or none of:
     * BOLD_FONT, UNDERLINE_FONT, ITALIC_FONT, PROP_FONT.
     */

    if (f & BOLD_FONT && f & ITALIC_FONT)
        curstyle = style_Alert;

    else if (f & BOLD_FONT)
        curstyle = style_Subheader;

    else if (f & UNDERLINE_FONT)
        curstyle = style_Emphasized;

    else if (f & ITALIC_FONT)
        curstyle = style_Emphasized;

    else
        curstyle = style_Normal;

    if (! ( f & PROP_FONT ) )
    {
        if ((ishtg || isworldbuilder) && wins[curwin].win && wins[curwin].win->type == wintype_TextBuffer) // || ismarjorie
        {
            return;
        }
            curstyle = style_Preformatted;
//        LOG("Changing to a fixed-width font\n");
        mono = true;

        // Hack to avoid extra line breaks in Captain Speedo intro boxes
        if (inwindow && wins[curwin].l > 2 && wins[curwin].t > 2)
        {
            glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);
        }
    } else mono = false;
}

int hugo_hasgraphics(void)
{
    return glk_gestalt(gestalt_Graphics, 0) && glk_gestalt(gestalt_DrawImage, wintype_TextBuffer);
}

int hugo_displaypicture(HUGO_FILE infile, long reslength)
{
    glui32 width, height, origwidth;
    int oldx = 0;

    if (hugo_hasgraphics())
    {
        if (mainwin)
        {
            struct winctx mainctx = wins[mainwin];
            if (mainctx.win && mainctx.win->type == wintype_TextBuffer) {
                LOG("  main x=%d y=%d\n", mainctx.curx, mainctx.cury);
                oldx = mainctx.x0 + mainctx.curx * gcellw;
            }
        }

        if (curwin == below_status && statuswin)
        {
            int diff = wins[statuswin].y1 - wins[curwin].y0;
            if (diff) {
                wins[curwin].y0 = wins[statuswin].y1;
                if (diff > 0 && curwin != mainwin)
                    wins[curwin].y1 = heglk_push_down_main_window_to(wins[curwin].y1 + diff);
                heglk_sizeifexists(curwin);
            }
        }

        if (reslength != lastres)
        {
            win_loadimage(0, infile->filename, glk_stream_get_position(infile), reslength);
            lastres = reslength;
        }

        win_sizeimage(&width, &height);

        origwidth = width;

        if (isfutureboy && width == 600 && height == 320)
            showing_futureboy_title = true;
        else
            showing_futureboy_title = false;

        if (ishtg && (reslength == 3501 || reslength == 4117))
            showing_author_photo = true;

        LOG("hugo_displaypicture: original image size: width %d height %d , reslength: %ld\n", width, height, reslength);
        float aspect = (float) height / width;

        int x0 = (wins[curwin].l - 1) * gcellw;
        int y0 = wins[curwin].y0;
        int winwidth = (wins[curwin].r - wins[curwin].l + 1) * gcellw + 1;
        int x1 = x0 + winwidth;
        if (x1 > gscreenw)
            x1 = gscreenw;
        int y1 = wins[curwin].y1;

        if (reslength == 14236 && isfutureboy)
        {
            second_image_row = curwin;
        }

        if (second_image_row && second_image_row != curwin && wins[second_image_row].y0 < y1)
        {
            y1 = wins[second_image_row].y0;
        }

        if (gscreenw - x1 < (gcellw * 2 + 2 * ggridmarginx) || wins[curwin].peggedtoright || x1 > gscreenw)
        {
            if (x0 < ggridmarginx)
            {
                x0 = 0;
            } else if (wins[curwin].halfscreenwidth)
            {
                x0 = gscreenw / 2;
            } else {
                int diff = gscreenw - x1;
                x0 += diff;
            }
            x1 = gscreenw;
        }

        if (wins[curwin].halfscreenwidth && x0 < ggridmarginx)
        {
            x0 = 0;
            x1 = gscreenw / 2 + 1;
        }

        // Hack to center the title mugshot gallery in Guilty Bastards
        if (isguiltybastards && wins[curwin].r - wins[curwin].l < screenwidth_in_chars / 4) {
            int diff = ggridmarginx + gcellw;
            x0 += diff;
            x1 += diff;
        }

        LOG("hugo_displaypicture curwin (%d) x0: %d y0: %d x1: %d y1: %d \n", curwin, wins[curwin].x0, wins[curwin].y0, wins[curwin].x1, wins[curwin].y1);

        if (width < 1 || height < 1)
            return false;

        if (width > x1 - x0) {
            width = x1 - x0;
            height = width * aspect;
        }

        if (y1 == y0) {
            y1 = y0 + (wins[curwin].b - wins[curwin].t) * gcellh;
            if (y1 > gscreenh)
            {
                y1 = gscreenh;
                y0 = gscreenh - gcellh;
            }
        }
        if (height > y1 - y0)
        {
            height = y1 - y0;
            width = height / aspect;
        }

        int xoff = ((x1 - x0) - width) / 2;
        int yoff = ((y1 - y0) - height) / 2;

        LOG("hugo_displaypicture w=%d h=%d x offset: %d y offset: %d\n", width, height, xoff, yoff);

        hugo_unmapcleared();

        LOG("ABS(oldx - wins[curwin].x0) = %d  gcellw * 2 = %f\n", ABS(oldx - wins[curwin].x0),  gcellw * 2);
        LOG("ABS(wins[mainwin].y1 - wins[curwin].y1) = %d  gbufcellh * 2 = %f\n", ABS(wins[mainwin].y1 - wins[curwin].y1),  gbufcellh * 2);

        // Prints an inline image in the main text window instead of replacing it with a new graphics window.
        // This will break any graphical menus that expect mouse input with correct coordinates,
        // such as the main menu of Cryptozookeeper, so we'll need to check for that here.

        if ((mainwin && wins[mainwin].win && // We have a main window with Glk counterpart

             // The two lines below try to check if the main window current position and the
             // current window, i.e. the window Hugo just set for this image,
             // are close enough.

             ((ABS(oldx - wins[curwin].x0) < gbufcellw * 2 && // The difference between main window last printed
               // character position (inaccurate because it is a buffer
               // window but the position is calculated using grid cellwidth)
               // and current window left edge is less than two (grid) characters

               ABS(wins[mainwin].y1 - wins[curwin].y1) < gbufcellh * 2)  // The difference betwee main window bottom edge
              // and current window bottom edge is less than two
              // characters

              || istraveling)

             && !isczk && !isenceladus && !isnecrotic && !isfallacy)) {
            // In Cryptozookeeper, this breaks the main menu. Enceladus simply looks better
            // with images in their own windows, vertically centered.

            // Scale down again to buffer margins
            x0 = wins[mainwin].x0 + gbuffermarginx;
            x1 = wins[mainwin].x1 - gbuffermarginx;

            if (origwidth < x1 - x0) {
                width = origwidth;
                height = origwidth * aspect;
            } else if (width > x1 - x0) {
                width = x1 - x0;
                height = width * aspect;
            }

            int linecount = height / gbufcellh + 1;
            LOG("  PictureInText at line %d, window %d\n", linecount, mainwin);

            if (1) // nlbufwin == mainwin)
            {
                nlbufcnt -= linecount;
                if (nlbufcnt < 0)
                    nlbufcnt = 0;
            }

            hugo_flushnl();
            glk_set_window(wins[mainwin].win);

            // We use a centered style for the image and
            // give it its own paragraph
            glk_set_style(style_User1);
            glk_put_char(' ');
            glk_set_hyperlink(1);
            win_drawimage(wins[mainwin].win->peer, imagealign_InlineUp, 0, width, height);
            glk_set_hyperlink(0);
            wins[mainwin].clear = 0;
            glk_put_char('\n');
            glk_set_style(style_Normal);

            wins[curwin].clear = 1;
            wins[curwin].y1 = 0;
            wins[curwin].b = 0;

            if (second_image_row)
            {
                wins[mainwin].y0 = wins[second_image_row].y1;
            }
        }
        else
        {
            // Here we create a new graphic window instead
            if (wins[curwin].win)
            {
                LOG("  unmap old window %d for graphics\n", curwin);
                if (wins[curwin].win->type != wintype_Graphics && !wins[curwin].clear) {
                    LOG("  pausing to let the player read the text in window %d before we delete it (and swap to graphics)\n", curwin);
                    hugo_waitforkey();
                }
                gli_delete_window(wins[curwin].win);
                wins[curwin].win = 0;
            }

            if (isfutureboy && ((width > 30 && width < 50 && height > 9 && height < 15) || (screenwidth_in_chars < 60 && ABS(screenwidth_in_chars / 2 - width) < 3))) {
                future_boy_main_image = curwin;
                future_boy_main_image_right = (wins[curwin].l > screenwidth_in_chars / 2);
                LOG("  Decided that the current image is Future Boy main image (%d)\n", curwin);
                if (future_boy_main_image_right)
                    LOG("  and that it is right-aligned\n");
                else
                    LOG("  and that it is left-aligned\n");

                if (x1 - x0 < (wins[curwin].r - wins[curwin].l) * gcellw) {
                    x1 = (wins[curwin].r - wins[curwin].l) * gcellw + x0;
                    y1 = (wins[curwin].b - wins[curwin].t) * gcellh + y0;
                }
            }

            wins[curwin].win = gli_new_window(wintype_Graphics, 0);
            win_sizewin(wins[curwin].win->peer,
                        x0, y0,
                        x1, y1);

            wins[curwin].x0 = x0;
            wins[curwin].y0 = y0;
            wins[curwin].x1 = x1;
            wins[curwin].y1 = y1;

            int color = hugo_color(wins[curwin].bg);

            /* Avoid clear background behind images in Guilty Bastards */
            /* Can happen after changing settings */
            if (isguiltybastards)
                glk_window_set_background_color(wins[curwin].win, color);
            else
                win_maketransparent(wins[curwin].win->peer);

            LOG("hugo_displaypicture: fill a rect with background color %x: width %d, height %d\n", hugo_color(wins[curwin].bg), abs(x1-x0), abs(y1-y0));
            win_fillrect(wins[curwin].win->peer, color, 0, 0, abs(x1-x0), abs(y1-y0));
            LOG("hugo_displaypicture: draw image: x offset %d, y offset %d, width %d, height %d\n", xoff, yoff, width, height);
            win_drawimage(wins[curwin].win->peer, xoff, yoff, width, height);
            wins[curwin].clear = 0;

        }
    }

    glk_stream_close(infile, NULL);
    just_displayed_something = true;
    return true;
}

void PromptMore(void)
{
}

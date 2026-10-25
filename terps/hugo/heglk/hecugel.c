/*
 * hecugel.c: the Spatterlight (garglk-derived) Glk front end for the Hugo
 * engine. Hugo games position text in arbitrary character rectangles, so
 * this file maps them onto Glk buffer, grid and graphics windows, with a
 * number of per-game heuristics for layouts that don't map cleanly.
 *
 * Build with -DHEGLK_DEBUG to send the LOG() trace to stderr.
 */

#include <stdio.h>

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

#ifdef HEGLK_DEBUG
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

/* These evaluate their arguments twice, so don't pass function calls. */
#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define SWAP_INT(a,b) do { int swap_tmp = (a); (a) = (b); (b) = swap_tmp; } while (0)


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
#define HUGO_INFINITE (0x7000)

/* Window contexts. Games use a few dozen at most, so the recycling path in
   heglk_new_wincontext() is only reached when building with a small
   -DMAXWINS (the trace suite passes with 8). */
#ifndef MAXWINS
#define MAXWINS 256
#endif

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
    int colors_sent;       /* win has been sent the zcolors for sentfg/sentbg */
    int sentfg, sentbg;
    int halfscreenwidth;   /* is this window supposed to be half screen width?*/
    int peggedtoright;     /* is this window supposed to be attached to right edge?*/
    int wasmoved;          /* was this window moved when the player adjusted window size?*/
    int screenheight_at_creation;
    int screenwidth_at_creation;
    char *filename;
    size_t offset;
    size_t length;
};

/* Contexts start at 1. wins[0] is a dummy that never gets a Glk window
   (unless heglk_new_wincontext runs out), so curwin == 0 means "no window"
   and code that reads wins[curwin] then sees win == NULL. Writes to its
   cursor fields are harmless. */
static struct winctx wins[MAXWINS];
static int nwins = 0;
static int curwin = 0;

static int screenwidth_in_chars = 0;
static int screenheight_in_chars = 0;


static int curstyle = style_Normal;

static int screen_bg = -1;

static int mainwin = 0;
static int statuswin = 0;
static int menuwin = 0;

static int below_status = 0; // Typically main window
static int second_image_row = 0; // Typically a border image or line below the image area

static int guilty_bastards_graphics_win = 0;
static int guilty_bastards_aux_win = 0;

/* Future Boy! */

/* The side of the screen the main picture is on; the sub pictures and the
   compass go on the other side */
enum { FB_ALIGNMENT_NONE = -1, FB_ALIGNMENT_LEFT, FB_ALIGNMENT_RIGHT };

/* The kinds of window heglk_id_future_boy_images() recognises */
enum {
    FB_IMAGE_MAIN,
    FB_IMAGE_SUB_1,
    FB_IMAGE_SUB_2,
    FB_IMAGE_COMPASS,
    FB_IMAGE_LINE,
    FB_STATUS,
    FB_MAIN,
    FB_IMAGE_UNKNOWN = 99
};

/* Resource lengths of particular pictures */
#define FB_TITLE_RESLEN 17211         /* the title screen */
#define FB_SECOND_ROW_RESLEN 14236    /* the image starting the second row */

/* A narrow window up to this fraction of the screen (in Hugo rows) high is
   the compass; a taller one is a sub picture. */
#define FB_COMPASS_MAX_ROWS 0.15

/* Where the pictures go, as fractions of the screen in pixels */
#define FB_MAIN_PICTURE_WIDTH 0.53
#define FB_SUB_PICTURE_WIDTH 0.21
#define FB_COMPASS_HEIGHT 0.12

/* What heglk_id_future_boy_images() makes of a window */
struct fb_id {
    int type;
    int alignment;    /* for the picture types but the line, else NONE */
};

/* Is TYPE one of the picture windows (main, subs, compass, line)? */
#define FB_IS_IMAGE(type) ((type) != FB_IMAGE_UNKNOWN && (type) != FB_MAIN && (type) != FB_STATUS)

static int future_boy_graphics_alignment = FB_ALIGNMENT_LEFT;
static int future_boy_line = 0;

/*
 * Menu state. Hugo has no notion of a menu, so we guess from what the game
 * prints. The variables involved:
 *
 *   mono      The game asked for a fixed-width font (hugo_font). Monospace
 *             text in the main window is printed to a grid, not a buffer.
 *   inmenu    We guess the game is showing a menu: set when a grid becomes
 *             the main window (hugo_mapcurwin) or a fixed-width line starts
 *             with '[' (hugo_print); cleared when line input is requested
 *             outside a grid (hugo_getline) or a graphics window appears.
 *             Affects only key input (waiting, mouse requests, the "*"
 *             cursor) and the status-line layout.
 *   menuwin   Non-zero while the status window is split into status + menu
 *             (heglk_ensure_menu, entered from hugo_print or a full-screen
 *             hugo_settextwindow). The menu grid becomes mainwin and
 *             below_status. Cleared when the game opens a smaller window
 *             (hugo_settextwindow) or clears the menu (hugo_clearwindow).
 *   menutop   The screen row where the menu window starts; rows above it
 *             go to the status window (hugo_settextpos). Set at the first
 *             menu and never reset.
 *
 * So: normal (menuwin == 0, inmenu false); probably a menu, still drawn in
 * the ordinary windows (menuwin == 0, inmenu true); and split status + menu
 * (menuwin != 0, inmenu usually true).
 */
static int menutop = 0;

static int inmenu = false;
static int mono = false;

/* Games that need special handling, see heglk_identify_game() */
enum heglk_game_id {
    GAME_OTHER,
    GAME_COLOSSAL,        /* Colossal Cave */
    GAME_CRIMSON,         /* A Crimson Spring */
    GAME_CWB2,            /* The Clockwork Boy 2 */
    GAME_CZK,             /* Cryptozookeeper */
    GAME_ENCELADUS,
    GAME_FALLACY,         /* Fallacy of Dawn */
    GAME_FUTURE_BOY,      /* Future Boy! */
    GAME_GUILTY_BASTARDS,
    GAME_HTG,             /* Haunting the Ghosts */
    GAME_MARJORIE,        /* Will The Real Marjorie Hopkirk Please Stand Up? */
    GAME_NECROTIC,        /* Necrotic Drift */
    GAME_NEXTDAY,         /* The Next Day */
    GAME_TETRIS,          /* Hugo Tetris */
    GAME_TRAVELING,       /* Tales of the Traveling Swordsman */
    GAME_WORLDBUILDER,    /* World Builder */
};

static enum heglk_game_id current_game = GAME_OTHER;

static int showing_author_photo = false;
static int showing_futureboy_title = false;

static int keypress = 0;

static int just_displayed_something = false;
static int waited_since_display = false;
static int lowest_printed_position_in_status = 0;
static int in_arrange_event = false;

static int nlbufcnt;
static int nlbufwin;

#define CHARWIDTH 1

static void hugo_mapcurwin(void);
static void hugo_handlearrange(void);
static void heglk_free_wincontext(int i);
static int heglk_new_wincontext(void);
static void hugo_flushnl(void);
static void heglk_sizeifexists(int i);
static void heglk_zcolor(void);
static int heglk_push_down_main_window_to(int y);
static void heglk_ensure_menu(void);
enum heglk_edge { EDGE_BOTTOM, EDGE_RIGHT };
static int heglk_find_window_edge(enum heglk_edge edge, int pos);
static void heglk_adjust_guilty_bastards_windows(void);
static void heglk_adjust_future_boy_windows(void);
static int heglk_future_boy_type(int left, int top, int right, int bottom, int screenwidth, int screenheight);
static int heglk_future_boy_window_type(int i);

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

/* Called by FatalError() just before it exits: show the error, which would
   otherwise be lost with the process. ERR starts with a blank line. */
void heglk_printfatalerror(char *err)
{
    while (*err == '\n')
        err++;
    fprintf(stderr, "hugo: %s", err);
    win_showerror(err);
}

/* Convert Glk special keycodes: */
static int hugo_convert_key(int n)
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

/* The pixel height of ROWS lines of a grid window, margins included,
   rounded down. */
static int rows_to_height(int rows)
{
    return rows * gcellh + 2 * ggridmarginy;
}

/* glk_select(), handling any window rearrangement on the way. */
static void heglk_select(event_t *ev)
{
    glk_select(ev);
    if (ev->type == evtype_Arrange)
        hugo_handlearrange();
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
        heglk_select(&ev);
        if (ev.type == evtype_CharInput)
        {
            keypress = hugo_convert_key(ev.val1);
        }
    }
    glk_request_timer_events(0);

    if (current_game != GAME_TETRIS && wins[curwin].win && wins[curwin].win->char_request)
        glk_cancel_char_event(wins[curwin].win);

    return true;
}


/* ----------------------------------------------------------------------------- */

/*
 * Stubs, trivials, and no-ops.
 */

void *hugo_blockalloc(long num)
{
    return malloc(num);
}

void hugo_blockfree(void *block)
{
    free(block);
}

/* Engine calls with nothing to do under Glk */

void hugo_closefiles(void)
{
    /* Glk closes all files at glk_exit() */
}

void hugo_cleanup_screen(void)
{
    /* Glk windows go away with the process */
}

void hugo_scrollwindowup(void)
{
    /* Glk should look after this for us */
}

void PromptMore(void)
{
    /* Glk windows page their own output */
}

void hugo_stopvideo(void)
{
    /* No video: hugo_hasvideo() says false */
}

/* The id under which the GUI caches a picture: Hugo pictures have no
   number, so use where the resource ends in its file. Pictures from two
   resource files could collide, but games use one. */
static int heglk_image_id(int offset, int length)
{
    return offset + length;
}

/* The black room description window on the right half of the screen in
   Guilty Bastards, which must survive clears and cursor moves. */
static int is_gb_room_description_window(int i)
{
    return current_game == GAME_GUILTY_BASTARDS && wins[i].l >= screenwidth_in_chars / 2 && wins[i].halfscreenwidth;
}

/* Cryptozookeeper's main menu needs its pictures in windows of their own,
   which take the mouse clicks, and Enceladus, Necrotic Drift and Fallacy of
   Dawn look better with pictures in their own windows, vertically centred.
   Other games may get a picture inline in the main window when it is placed
   where the main window's text is going (see hugo_displaypicture). */
static int heglk_game_allows_inline_pictures(void)
{
    switch (current_game)
    {
        case GAME_CZK:
        case GAME_ENCELADUS:
        case GAME_NECROTIC:
        case GAME_FALLACY:
            return false;
        default:
            return true;
    }
}

/* Haunting the Ghosts and World Builder print their regular game text in
   the fixed-width font, so a fixed-width font in a buffer window is ignored
   there: the text stays proportional and wraps. */
static int heglk_game_ignores_fixed_font_in_buffer(void)
{
    return current_game == GAME_HTG || current_game == GAME_WORLDBUILDER;
}

/* Games that need special handling, recognised by the start of the title
   they set or, for games that set none, by their file name. */
struct heglk_game {
    const char *prefix;
    enum heglk_game_id id;
};

static const struct heglk_game games_by_title[] = {
    { "Hugo Tetris", GAME_TETRIS },
    { "CRYPTOZOOKEEPER", GAME_CZK },
    { "Future Boy!", GAME_FUTURE_BOY },
    { "ENCELADUS", GAME_ENCELADUS },
    { "NECROTIC DRIFT", GAME_NECROTIC },
    { "FALLACY OF DAWN", GAME_FALLACY },
    { "The Clockwork Boy 2", GAME_CWB2 },
    { "World Builder", GAME_WORLDBUILDER },
    { "The Next Day", GAME_NEXTDAY },
    { "A CRIMSON", GAME_CRIMSON },
    { "Guilty Bastards", GAME_GUILTY_BASTARDS },
    { "Tales of the Traveling", GAME_TRAVELING },
};

static const struct heglk_game games_by_filename[] = {
    { "colossal.hex", GAME_COLOSSAL },
    { "htgessay.hex", GAME_HTG },
    { "marjorie.hex", GAME_MARJORIE },
};

static void heglk_identify_game(const struct heglk_game *games, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!strncmp(name, games[i].prefix, strlen(games[i].prefix)))
        {
            current_game = games[i].id;
            return;
        }
    }
}

void hugo_setgametitle(char *t)
{
    if (!strncmp(t, "NOT READY YET", 13))
        return;

    garglk_set_story_title(t);

    heglk_identify_game(games_by_title, sizeof games_by_title / sizeof *games_by_title, t);
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

/*
 * Return true if a keypress is waiting to be retrieved.
 */

int hugo_iskeywaiting(void)
{
    return keypress;
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



/*
 * COLOURS
 *
 * The engine passes hugo_settextcolor() and hugo_setbackcolor() an index:
 * 0-15 are the Hugo palette, 16-19 the four defaults (text, background,
 * statusline text, statusline background: the DEF_* names above) and 20
 * means "match the foreground". A window context keeps the pair last set
 * in it (fg, bg) and the colour it is painted in (papercolor), because
 * the engine does not send the main window's colours again when a window
 * block ends.
 *
 * Text is coloured with zcolors. A default that the window treats as its
 * own, the main pair in a text buffer or the statusline pair in a grid or
 * the status window, goes to the GUI as zcolor_Default, which it resolves
 * against the theme when it draws, so that the text follows a light/dark
 * switch. Everything else is sent as RGB: the palette, the defaults of
 * the other kind of window, and any default in a context without a Glk
 * window. heglk_rgb_of() makes that decision and heglk_zcolors_of() the
 * pair for printing, both from a heglk_colorctx that describes the
 * window, so the rules can be read without the window array.
 *
 * An explicit background is painted on the whole window with win_setbgnd
 * (heglk_set_window_bg, hugo_mapcurwin), and on the screen behind the
 * windows when the window fills it (hugo_clearwindow, which records it
 * in screen_bg for the contexts opened afterwards). A context remembers
 * the pair its Glk window was last sent (colors_sent, sentfg, sentbg),
 * so hugo_print only sends zcolors on a change; painting or mapping the
 * window forgets them.
 */

/* The 16 standard Hugo colours, HUGO_BLACK to HUGO_BRIGHT_WHITE */
static const int hugo_rgb[16] = {
    0x000000, 0x00007f, 0x007f00, 0x007f7f,
    0x7f0000, 0x7f007f, 0x7f5f00, 0xcfcfcf,
    0x3f3f3f, 0x0000ff, 0x00ff00, 0x00ffff,
    0xff0000, 0xff00ff, 0xffff00, 0xffffff,
};

/* Which pair of defaults a window context treats as its own */
enum heglk_defaults { DEFAULTS_NONE, DEFAULTS_MAIN, DEFAULTS_STATUS };

/* What the colour rules need to know about a window context */
struct heglk_colorctx
{
    enum heglk_defaults defaults;
    glui32 wintype;        /* its Glk window type, 0 when unmapped */
    int fg, bg;            /* the colours set in it */
    int papercolor;        /* the colour it is painted in */
};

static struct heglk_colorctx heglk_colorctx_of(int i)
{
    struct heglk_colorctx ctx;

    ctx.defaults = DEFAULTS_NONE;
    ctx.wintype = 0;
    if (wins[i].win || (statuswin && i == statuswin))
        ctx.defaults = (i == statuswin || wins[i].win->type == wintype_TextGrid)
                     ? DEFAULTS_STATUS : DEFAULTS_MAIN;
    if (wins[i].win)
        ctx.wintype = wins[i].win->type;
    ctx.fg = wins[i].fg;
    ctx.bg = wins[i].bg;
    ctx.papercolor = wins[i].papercolor;
    return ctx;
}

/* Colour index C as the GUI gets it, for the window CTX describes: a
 * default that the window treats as its own, and that is the colour set
 * in it, goes as zcolor_Default, the rest as RGB. Colour 20, match the
 * foreground, is resolved by the setters below; it only arrives here from
 * hugo_clearfullscreen, which passes the engine's own bgcolor. */
static int heglk_rgb_of(const struct heglk_colorctx *ctx, int c)
{
    int converted = 0;

    switch (c)
    {
        case DEF_FCOLOR:     converted = gfgcol; break;
        case DEF_BGCOLOR:     converted = gbgcol; break;
        case DEF_SLFCOLOR:     converted = gsfgcol; break;
        case DEF_SLBGCOLOR:     converted = gsbgcol; break;
        case 20:    /* match foreground (guarded against recursing for ever) */
            converted = fcolor == 20 ? 0 : heglk_rgb_of(ctx, fcolor); break;
        default:
            if (c >= 0 && c < 16)
                converted = hugo_rgb[c];
            else
                LOG("Unmapped color %d\n", c);
    }

    /* A default text color used to be passed as a default only when the
       background was a default too; with a custom background it was fixed
       at gfgcol/gsfgcol, and that text kept the old side's color after a
       theme switch. */
    switch (ctx->defaults)
    {
        case DEFAULTS_STATUS:
            if ((c == DEF_SLFCOLOR && ctx->fg == DEF_SLFCOLOR)
                || (c == DEF_SLBGCOLOR && ctx->bg == DEF_SLBGCOLOR))
                return zcolor_Default;
            break;
        case DEFAULTS_MAIN:
            if ((c == DEF_FCOLOR && ctx->fg == DEF_FCOLOR)
                || (c == DEF_BGCOLOR && ctx->bg == DEF_BGCOLOR))
                return zcolor_Default;
            break;
        case DEFAULTS_NONE:
            break;
    }

    return converted;
}

/* Colour C as the GUI gets it, for window context I */
static int heglk_color_for(int i, int c)
{
    struct heglk_colorctx ctx = heglk_colorctx_of(i);
    return heglk_rgb_of(&ctx, c);
}

int hugo_color(int c)
{
    return heglk_color_for(curwin, c);
}

struct heglk_zcolors { int fg, bg; };

/* The zcolor pair for printing in the window CTX describes */
static struct heglk_zcolors heglk_zcolors_of(const struct heglk_colorctx *ctx)
{
    struct heglk_zcolors z;
    int shown_fg, shown_bg;

    z.fg = heglk_rgb_of(ctx, ctx->fg);
    z.bg = heglk_rgb_of(ctx, ctx->bg);

    /* What the text will look like, with a buffer's defaults resolved
       against the theme */
    shown_fg = z.fg;
    shown_bg = z.bg;
    if (ctx->wintype == wintype_TextBuffer)
    {
        if (shown_fg < 0)
            shown_fg = gfgcol;
        if (shown_bg < 0)
            shown_bg = gbgcol;
    }

    /* The statusline default background on a grid painted in something
       else is the statusline colour, not the paint */
    if (ctx->wintype == wintype_TextGrid && ctx->bg == DEF_SLBGCOLOR
        && ctx->papercolor != DEF_SLBGCOLOR)
        z.bg = gsbgcol;

    /* Both defaults: leave the stream's colours as they are */
    if (z.fg == (int)zcolor_Default && z.bg == (int)zcolor_Default)
        z.fg = z.bg = (int)zcolor_Current;

    /* Black on black would vanish, so show it in white */
    if (shown_fg == 0 && (shown_bg == 0 || (z.bg < 0 && ctx->papercolor == 0)))
        z.fg = heglk_rgb_of(ctx, HUGO_WHITE);

    return z;
}

void hugo_settextcolor(int c)
{
    if (!curwin)
        return;
    if (c == 20)    /* match foreground: it already does */
        return;
    if (c < 0 || c > DEF_SLBGCOLOR)
        c = DEF_FCOLOR;
    wins[curwin].fg = c;
}


void hugo_setbackcolor(int c)
{
    if (!curwin)
        return;
    if (c == 20)    /* match foreground */
        c = wins[curwin].fg;
    if (c < 0 || c > DEF_SLBGCOLOR)
        c = DEF_BGCOLOR;
    wins[curwin].bg = c;
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

    /* FORCED_SPACE is a control code that prints as ' ' */
    if (a == FORCED_SPACE || (unsigned char)a >= ' ')
        return CHARWIDTH;

    return 0;
}

/* The width of A in characters, skipping colour and font codes */
static int heglk_textlen(char *a, int (*width)(char))
{
    int i, slen, len = 0;

    slen = (int)strlen(a);

    for (i=0; i<slen; i++)
    {
        if (a[i]==COLOR_CHANGE) i+=2;
        else if (a[i]==FONT_CHANGE) i++;
        else
            len += width ? width(a[i]) : 1;
    }

    return len;
}

int hugo_textwidth(char *a)
{
    return heglk_textlen(a, hugo_charwidth);
}

int hugo_strlen(char *a)
{
    return heglk_textlen(a, NULL);
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
    int width = (int)gscreenw - 2 * ggridmarginx;
    if (width < 0)
        width = 0;
    width = width / gcellw;
    width -= 2; /* -2 to trigger automatic line wrapping */
    if (width < 0)
        width = 0;
    return width;
}

int heglk_get_screenheight(void)
{
    int height = ((int)gscreenh - 2 * ggridmarginy) / gcellh;
    if (height < 0)
        height = 0;
    return height;
}

/* ----------------------------------------------------------------------------- */

void hugo_init_screen(void)
{

    /* Does whatever has to be done to initially set up the display. */

    /* By setting the width and height so high, we're basically
     forcing the Glk library to deal with text-wrapping and
     page ends
     */

    SCREENWIDTH = 0x7fff;
    SCREENHEIGHT = 0x7fff;
    FIXEDCHARWIDTH = 1;
    FIXEDLINEHEIGHT = 1;

    gli_block_rearrange = 1;

    screenwidth_in_chars = heglk_get_linelength();
    screenheight_in_chars = heglk_get_screenheight();

    nwins = 1;
    curwin = 0;

    nlbufwin = 0;
    nlbufcnt = 0;

    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Justification, stylehint_just_Centered);

    game_version = HEVERSION * 10 + HEREVISION;

    hugo_clearfullscreen();

    if (mainwin)
        wins[mainwin].bg = HUGO_BLACK;

    hugo_clearwindow();

    win_setbgnd(-1, 0);

    heglk_identify_game(games_by_filename, sizeof games_by_filename / sizeof *games_by_filename, gli_story_name);
}

static void heglk_record_physical(const struct winctx *ctx) {
    /* record position of the window */
    physical_windowleft = (ctx->l-1)*FIXEDCHARWIDTH;
    physical_windowtop = (ctx->t-1)*FIXEDLINEHEIGHT;
    physical_windowright = ctx->r*FIXEDCHARWIDTH-1;
    physical_windowbottom = ctx->b*FIXEDLINEHEIGHT-1;
    if (ctx->r - ctx->l < screenwidth_in_chars)
        physical_windowwidth = (ctx->r-ctx->l-(ggridmarginx / gcellw - 1))*FIXEDCHARWIDTH;
    else
        physical_windowwidth = (ctx->r-ctx->l+1)*FIXEDCHARWIDTH;
    if (ctx->isaux && ctx->l > 3 && ctx->r > screenwidth_in_chars - 2)
        physical_windowwidth = SCREENWIDTH;

    if (physical_windowwidth < 0)
        physical_windowwidth = 0;

    physical_windowheight = (ctx->b-ctx->t+1)*FIXEDLINEHEIGHT;
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

    if (!curwin || !wins[curwin].win)
    {
        LOG("hugo_getline: no window to read from!\n");
        buffer[0] = '\0';
        return;
    }

    glk_set_window(wins[curwin].win);
    /* Print prompt */
    hugo_print(prompt);

    // Asking for line input usually means that we are not in a menu
    // But there is an exception in Haunting the Ghosts.
    if (wins[curwin].win->type != wintype_TextGrid)
    {
        inmenu = false;
    }

    /* Request line input */
    if (wins[curwin].win->char_request)
        glk_cancel_char_event(wins[curwin].win);

    glk_request_line_event(wins[curwin].win, buffer, MAXBUFFER, 0);

    while (ev.type != evtype_LineInput)
        heglk_select(&ev);

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


/* Ask the Glk window of context I for a key, and outside menus for a click
   or hyperlink as well. Returns false if there is no window to take a key.
   A context asked for a click is marked clear, so that what is shown in it
   is not waited on again before it is replaced (see hugo_print and
   hugo_displaypicture). */
static int heglk_request_input(int i)
{
    window_t *win = wins[i].win;
    int event_requested = 0;

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
                    wins[i].clear = true;
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

    return event_requested;
}

static void heglk_cancel_all_requests(void)
{
    for (window_t *win = glk_window_iterate(NULL, NULL); win; win = glk_window_iterate(win, NULL))
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
}

/* hugo_waitforkey
 *
 * Provided to be replaced by multitasking systems where cycling while
 * waiting for a keystroke may not be such a hot idea.
 */

int hugo_waitforkey(void)
{
    event_t ev = {0};

    LOG("hugo_waitforKey\n");

    if (keypress)
    {
        int oldkeypress = keypress;
        keypress = 0;
        return oldkeypress;
    }

    /* Ignore calls where there's been no recent display, unless a key has
       already been waited for since the last one: a game that pauses in a
       loop until it gets a key it wants (the hint screens of Future Boy!)
       would otherwise spin there for ever, never asking Glk for a key.

         just_displayed_something  inmenu  waited_since_display  result
         (keypress pending)           -       -                   that key
         false                      false   false                 0, no wait
         true                         -       -                   wait
         -                          true      -                   wait
         -                            -     true                  wait

       just_displayed_something is set by printing and pictures and cleared
       here; waited_since_display is set here and cleared by printing,
       pictures and clearing a window or the screen. */
    if (!just_displayed_something && !inmenu && !waited_since_display)
        return 0;

    just_displayed_something = false;
    waited_since_display = true;

    int event_requested = heglk_request_input(curwin);

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
        heglk_select(&ev);

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

    heglk_cancel_all_requests();

    /* Convert Glk special keycodes: */
    return hugo_convert_key (ev.val1);
}


/* Delete window context I's Glk window, if it has one. */
static void heglk_unmap_window(int i)
{
    if (wins[i].win)
        gli_delete_window(wins[i].win);
    wins[i].win = NULL;
}

static void heglk_unmap_and_mark_clear(int i)
{
    heglk_unmap_window(i);
    wins[i].clear = 1;
}

/* Context I is about to lose what it shows. If that was not cleared since
   it was printed or drawn, the player may not have seen it yet, so wait
   for a key first. */
static void heglk_pause_before_discard(int i)
{
    if (!wins[i].clear)
        hugo_waitforkey();
}

/* Forget the picture last drawn in window context I. */
static void heglk_clear_filename(int i)
{
    free(wins[i].filename);
    wins[i].filename = NULL;
}

/* Paint the Glk window of context I in its background colour. */
static void heglk_set_window_bg(int i)
{
    wins[i].papercolor = wins[i].bg;
    wins[i].colors_sent = false;
    win_setbgnd(wins[i].win->peer, heglk_color_for(i, wins[i].bg));
}

/* Drop any Glk window and heap data belonging to a window context,
 * and reset it so that it can be handed out again by
 * heglk_new_wincontext().
 */

static void heglk_free_wincontext(int i)
{
    if (i < 1 || i >= MAXWINS)
        return;

    heglk_unmap_window(i);
    heglk_clear_filename(i);

    memset(&wins[i], 0, sizeof(struct winctx));
}

/* Hand out a window context.
 *
 * hugo_unmapcleared() only reclaims a context when it happens to sit at the
 * top of the array, so nwins otherwise grows for every new window rectangle
 * a game defines, and would eventually run off the end of wins[]. Once we
 * are out of slots, recycle a spent context instead: one that has been
 * cleared, has no Glk peer, and isn't one of the windows we track by index.
 * (A cleared context can still be matched by rectangle and reused, so we
 * only take one when there is nothing left to hand out.)
 *
 * Returns 0 if even that fails; callers already treat a zero curwin as
 * "no window".
 */

static int heglk_new_wincontext(void)
{
    if (nwins < MAXWINS)
        return nwins++;

    for (int i = 1; i < nwins; i++)
    {
        if (wins[i].win || !wins[i].clear)
            continue;
        if (i == mainwin || i == statuswin || i == menuwin || i == nlbufwin
            || i == below_status || i == second_image_row || i == future_boy_line
            || i == guilty_bastards_graphics_win || i == guilty_bastards_aux_win)
            continue;

        LOG("Recycled window context %d\n", i);
        heglk_free_wincontext(i);
        return i;
    }

    LOG("Out of window contexts!\n");
    return 0;
}

/* Clear everything on the screen,
 * move the cursor to the top-left
 * corner of the screen
 */

void hugo_clearfullscreen(void)
{
    int i;

    LOG("hugo_clearfullscreen\n");

    /* What a key was waited for on is gone: a pause on the empty screen is
       skipped again (see hugo_waitforkey). */
    waited_since_display = false;
    nlbufwin = 0;
    nlbufcnt = 0;

    for (i = 1; i < nwins; i++)
    {
        heglk_free_wincontext(i);
    }

    nwins = 1;
    curwin = 0;
    mainwin = 0;
    statuswin = 0;
    below_status = 0;
    second_image_row = 0;
    future_boy_line = 0;
    guilty_bastards_graphics_win = 0;
    guilty_bastards_aux_win = 0;

    /* create initial fullscreen window */
    hugo_settextwindow(1, 1,
                       SCREENWIDTH,
                       SCREENHEIGHT);

    win_setbgnd(-1, hugo_color(bgcolor));

    currentpos = 0;
    currentline = 1;
}

// Returns true if the window context covers the screen
static int isfullscreen(const struct winctx *ctx)
{
    return ((ctx->x0 == 0 && ctx->y0 == 0 && ctx->x1 == gscreenw && ctx->y1 == gscreenh)
         || (ctx->l == 1 && ctx->r == SCREENWIDTH && ctx->t == 1 && ctx->b == SCREENHEIGHT));
}

static int heglk_clear_future_boy_type(int type, int not_this) {
    if (current_game != GAME_FUTURE_BOY)
        return 0;
    if (!FB_IS_IMAGE(type) || type == FB_IMAGE_LINE)
        return 0;
    int result = 0;

    for (int i = 1; i < nwins; i++)
    {
        if (i == not_this)
            continue;
        else if (heglk_future_boy_window_type(i) == type) {
            heglk_unmap_and_mark_clear(i);
            heglk_clear_filename(i);
            result = 1;
        }
    }
    return result;
}

/* Clear context i, inside the window being cleared (curwin), and paint it
 * in curwin's background colour. A cleared context is reused or unmapped
 * by the next hugo_settextwindow (hugo_unmapcleared). */
static void heglk_clear_context(int i)
{
    if (wins[i].win)
    {
        if (wins[curwin].win && hugo_color(wins[curwin].bg) != -1)
        {
            wins[i].bg = wins[curwin].bg;
            heglk_set_window_bg(i);
        }
        LOG("   cleared %d\n", i);
        glk_window_clear(wins[i].win);

        // Find whatever now sits just below the status window.
        if (i == below_status)
        {
            below_status = 0;
            if (mainwin)
            {
                below_status = heglk_find_window_edge(EDGE_BOTTOM, wins[mainwin].t);
                if (below_status == statuswin || wins[below_status].clear)
                    below_status = 0;
            }
        }
        if (i == second_image_row)
            second_image_row = 0;
    }

    wins[i].clear = 1;
    wins[i].curx = 0;
    wins[i].cury = 0;
    wins[i].isaux = 0;
}

/* Clear the currently defined window, move the cursor to the top-left
 * corner of the window.
 */

void hugo_clearwindow(void)
{
    waited_since_display = false;

    if (menuwin) {
        return;
    }

    if (!curwin)
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

    if (current_game == GAME_FUTURE_BOY) {
        int type = heglk_future_boy_type(l, t, r, b, screenwidth_in_chars, screenheight_in_chars);
        if (heglk_clear_future_boy_type(type, -1))
            return;
    }

    if (curwin == nlbufwin)
    {
        nlbufwin = 0;
        nlbufcnt = 0;
    }

    /* We delete the main window here in order to be able */
    /* to change input text color */
    if (curwin == mainwin && !showing_futureboy_title)
        heglk_unmap_window(curwin);

    for (i = 1; i < nwins; i++)
    {
        if (wins[i].l >= l && wins[i].t >= t && wins[i].r <= r && wins[i].b <= b)
        {
            if (i == future_boy_line) {
                int line = wins[future_boy_line].t;
                if (abs(line - b) < 3 || abs(line - t) < 3)
                    continue;
            } else future_boy_line = 0;

            heglk_clear_context(i);
        }
    }

    if (wins[curwin].win)
        heglk_set_window_bg(curwin);

    // Hack to keep black room description window from
    // being deleted at the first turn of Guilty Bastards (trace: guilty-*)
    if (is_gb_room_description_window(curwin))
    {
        wins[curwin].clear = 0;
    }

    if (isfullscreen(&wins[curwin]))
    {
        win_setbgnd(-1, hugo_color(wins[curwin].bg));
        screen_bg = wins[curwin].bg;
        LOG("Window %d filled the screen, so we set the entire window to its background color.\n", curwin);
    }

    currentpos = 0;
    currentline = 1;
}

/* Unmap every cleared window except the current one, and forget the roles
 * they had. mainwin keeps its role: it is mapped again on the next print.
 * menuwin is never cleared while set (hugo_clearwindow returns early, and
 * leaving the menu resets menuwin first), and future_boy_line is reset by
 * hugo_clearwindow. */
static void hugo_unmapcleared(void)
{
    int i;

    for (i = 1; i < nwins; i++)
    {
        if (i != curwin && wins[i].clear && wins[i].win)
        {
            LOG(" + unmap window %d\n", i);
            heglk_unmap_window(i);
            heglk_clear_filename(i);
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

/* A half-screen-wide window keeps its half after a resize. In Guilty
   Bastards it is the room description, which sits below the status line. */
static void heglk_rearrange_half_screen(int i)
{
    if (current_game == GAME_GUILTY_BASTARDS && i != statuswin)
        below_status = i;
    else if (wins[i].peggedtoright)
        wins[i].x0 = gscreenw / 2;
    else
        wins[i].x1 = wins[i].x0 + gscreenw / 2;
}

/* Cryptozookeeper: an aux window sticks to the window on its left and
   ends where the main window begins. */
static void heglk_rearrange_czk_aux(int i)
{
    int toleft = heglk_find_window_edge(EDGE_RIGHT, wins[i].l);
    if (toleft)
        wins[i].x0 = wins[toleft].x1;
    wins[i].y1 = wins[mainwin].y0;
    if (wins[i].win && wins[i].win->type == wintype_TextBuffer)
        wins[i].x1 = gscreenw;
}

static void hugo_handlearrange(void)
{
    int width = heglk_get_linelength();
    int height = heglk_get_screenheight();

    /* The pixel test never holds, as heglk_get_linelength() leaves two
       columns spare, so in practice this never returns early. */
    if (width == screenwidth_in_chars && height == screenheight_in_chars && gcellw * screenwidth_in_chars + ggridmarginx * 2 == gscreenw) {
        return;
    }

    screenwidth_in_chars = width;
    screenheight_in_chars = height;

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
            wins[statuswin].y1 = rows_to_height(wins[statuswin].b);
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

                if (wins[i].halfscreenwidth && current_game != GAME_FUTURE_BOY)
                    heglk_rearrange_half_screen(i);
                if (current_game == GAME_CZK && wins[i].isaux)
                    heglk_rearrange_czk_aux(i);

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

    heglk_adjust_future_boy_windows();
    heglk_adjust_guilty_bastards_windows();
}

/* Fresh state for a context just returned by heglk_new_wincontext().
   The position and the size at creation are set by the caller. */
static void heglk_init_wincontext(int i, int type)
{
    int status = (type == ISSTATUS);

    wins[i].win = NULL;
    wins[i].clear = 1;
    wins[i].wasmoved = false;
    wins[i].filename = NULL;
    wins[i].fg = status ? DEF_SLFCOLOR : DEF_FCOLOR;
    if (screen_bg != -1)
        wins[i].bg = screen_bg;
    else
        wins[i].bg = status ? DEF_SLBGCOLOR : DEF_BGCOLOR;
    wins[i].papercolor = wins[i].bg;
    wins[i].colors_sent = false;
}

/* Shrinks *w x *h to fit a boxw x boxh box, keeping the aspect ratio,
   and centres it. Images smaller than the box are not scaled up. */
static void heglk_fit_image(int boxw, int boxh, glui32 *w, glui32 *h, int *xoff, int *yoff)
{
    float aspect = (float) *h / *w;

    if (*w > boxw) {
        *w = boxw;
        *h = *w * aspect;
    }
    if (*h > boxh) {
        *h = boxh;
        *w = *h / aspect;
    }
    *xoff = (boxw - *w) / 2;
    *yoff = (boxh - *h) / 2;
}

static void heglk_sizeifexists(int i)
{
    if (wins[i].win)
    {
        if ( wins[i].x0 < 0)
        {
            wins[i].x0 = 0;
        }
        if (wins[i].y0 > wins[i].y1)
            SWAP_INT(wins[i].y0, wins[i].y1);
        if (wins[i].x0 > wins[i].x1)
            SWAP_INT(wins[i].x0, wins[i].x1);

        if (wins[i].y0 == wins[i].y1)
        {
            LOG("glk_sizeifexists: Window %d was resized to height 0! Leave it at one character high.\n", i);
            wins[i].y1 = wins[i].y0 + gcellh;
        }
        win_sizewin(wins[i].win->peer, wins[i].x0, wins[i].y0, wins[i].x1, wins[i].y1);
    }
}

/* Pre-v2.4 versions had no proper concept of windows, */
/* so we guess the status window height here and in */
/* heglk_adjust_pre_v24_status_when_printing() below. */

static int heglk_adjust_pre_v24_status_when_setting_window(int bottom)
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

static int heglk_push_down_main_window_to(int y)
{
    LOG("heglk_push_down_main_window_to %d (main window:%d)\n", y, mainwin);
    if (!mainwin || !y)
    {
        return y;
    }

    if (statuswin && wins[statuswin].y1)
        y = MAX(y, wins[statuswin].y1);

    if (second_image_row && wins[second_image_row].y1)
        y = MIN(y, wins[second_image_row].y1);

    if (current_game == GAME_GUILTY_BASTARDS && y - wins[mainwin].y0 > gcellh * 10)
        return wins[mainwin].y0;

    int minimal_mainwin_height = gbuffermarginy * 2 + gbufcellh * 3;
    if (wins[mainwin].win && wins[mainwin].win->type == wintype_TextGrid)
        minimal_mainwin_height = rows_to_height(3);
    if (gscreenh <  minimal_mainwin_height)
        minimal_mainwin_height = gscreenh / 2;
    if (gscreenh - y < minimal_mainwin_height)
        y = gscreenh - minimal_mainwin_height;
    /* Dont't allow negative height */
    if (wins[mainwin].y1 <= y)
        return wins[mainwin].y0;
    wins[mainwin].y0 = y;
    heglk_sizeifexists(mainwin);
    LOG("Pushed down main window to %d\n", y);
    if (!y) {
        LOG("y == 0!\n");
    }
    return y;
}

/* The first window whose bottom (EDGE_BOTTOM) or right (EDGE_RIGHT) edge is
 * within one cell of POS, i.e. the window just above or to the left of
 * something starting at POS. Cleared windows and the caller's own window are
 * not skipped; callers that care check the result. */
static int heglk_find_window_edge(enum heglk_edge edge, int pos)
{
    for (int i = 1; i < nwins; i++)
    {
        int e = (edge == EDGE_BOTTOM ? wins[i].b : wins[i].r);
        if (ABS(e - pos) <= 1) {
            LOG("Window %d (l:%d, t:%d, r:%d, b:%d) has its %s edge at %d\n", i, wins[i].l, wins[i].t, wins[i].r, wins[i].b, edge == EDGE_BOTTOM ? "bottom" : "right", pos);
            return i;
        }
    }
    return 0;
}

static void heglk_ensure_menu(void)
{
    LOG("heglk_ensure_menu\n");
    LOG("Splitting status window into status + menu\n");

    // We only want two windows, so delete all others
    for (int i = 1; i < nwins; i++) {
        if (i != menuwin && i != statuswin)
        {
            heglk_unmap_and_mark_clear(i);
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

    wins[menuwin].y0 = rows_to_height(menutop - 1 - (menutop > 2));
    wins[menuwin].y1 = gscreenh;

    if (statuswin == menuwin)
    {
        statuswin = 0;
    }
    if (statuswin)
    {
        wins[menuwin].fg = wins[statuswin].fg;
        wins[menuwin].bg = wins[statuswin].bg;
        heglk_set_window_bg(menuwin);
    } else {
        statuswin = heglk_new_wincontext();
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
    // in Clockwork Boy 2 and The Next Day (traces: cb2-*, nextday-*)
    if (!in_arrange_event && (current_game == GAME_CWB2 || current_game == GAME_NEXTDAY))
    {
        glk_window_clear(wins[menuwin].win);
    }
    glk_window_move_cursor(wins[menuwin].win, 0, 0);
}

/* Guilty Bastards: the picture on the left half below the status line, the
   room description beside it, and the main window below both. Called after
   any window is set and after an arrange event; like
   heglk_adjust_future_boy_windows(), it sizes the Glk windows it moves
   itself, after the generic code has placed them. */
static void heglk_adjust_guilty_bastards_windows(void)
{
    if (!(current_game == GAME_GUILTY_BASTARDS && guilty_bastards_graphics_win && guilty_bastards_aux_win && statuswin && mainwin))
        return;

    for (int i = 1; i < nwins; i ++)
    {
        if (wins[i].win && wins[i].win->type == wintype_Graphics && i != guilty_bastards_graphics_win)
        {
            heglk_unmap_and_mark_clear(i);
        }
    }

    int diff = wins[statuswin].y1 - wins[guilty_bastards_graphics_win].y0;

    wins[guilty_bastards_graphics_win].y0 = wins[statuswin].y1;
    wins[guilty_bastards_graphics_win].y1 += diff;

    /* If the picture window's width no longer suits the screen (it was set
       on a wider one, or is under a third of this one), give it a bottom
       edge of half the screen *width*: the room pictures are landscape and
       fill the left half, so that is a little more than they need. Taken
       from the width on purpose; it fires on most resizes. */
    if (gscreenw < wins[guilty_bastards_graphics_win].x1 || gscreenw > wins[guilty_bastards_graphics_win].x1 * 3)
        wins[guilty_bastards_graphics_win].y1 = gscreenw / 2;

    /* Leave room for the main window: y1 + y0 is the bottom edge plus as
       much again as the status line takes */
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

    if (wins[guilty_bastards_graphics_win].win)
        glk_window_set_background_color(wins[guilty_bastards_graphics_win].win, heglk_color_for(guilty_bastards_graphics_win, wins[guilty_bastards_graphics_win].bg));

    heglk_sizeifexists(guilty_bastards_graphics_win);
    heglk_sizeifexists(guilty_bastards_aux_win);
}

static int fb_alignment(int main_picture_on_right)
{
    return main_picture_on_right ? FB_ALIGNMENT_RIGHT : FB_ALIGNMENT_LEFT;
}

/* Classify a Future Boy! window by its place on a screen of SCREENWIDTH x
   SCREENHEIGHT characters, and for a picture, the side the main picture
   is on. */
static struct fb_id heglk_id_future_boy_images(int left, int top, int right, int bottom, int screenwidth, int screenheight) {
    struct fb_id id = { FB_IMAGE_UNKNOWN, FB_ALIGNMENT_NONE };

    if (right >= HUGO_INFINITE)
        right = screenwidth;

    int width = right - left;
    int height = bottom - top;

    if (bottom >= HUGO_INFINITE) {
        id.type = FB_MAIN;
    } else if (top > 3 && top == bottom && left == 1 && right == screenwidth) {
        id.type = FB_IMAGE_LINE;
    } else if (top > screenheight / 2) {
        LOG("Did not recognize this as a Future boy image.\n");
    } else if (width < screenwidth && width >= screenwidth / 2) {
        id.type = FB_IMAGE_MAIN;
        id.alignment = fb_alignment(left != 1);
    } else if (width <= screenwidth / 4 && height > screenheight * FB_COMPASS_MAX_ROWS) {
        /* A sub picture: against the edge of the screen, or between the
           main picture and that edge */
        if (right == screenwidth || left == 1) {
            id.type = FB_IMAGE_SUB_2;
            id.alignment = fb_alignment(left == 1);
        } else {
            id.type = FB_IMAGE_SUB_1;
            id.alignment = fb_alignment(left < screenwidth / 2);
        }
    } else if (top == 2 && width <= screenwidth / 4 && height <= screenheight * FB_COMPASS_MAX_ROWS) {
        id.type = FB_IMAGE_COMPASS;
        id.alignment = fb_alignment(left < 3);
    } else if (top == 1 && bottom < 3) {
        id.type = FB_STATUS;
    } else {
        LOG("Did not recognize this as a Future boy image.\n");
    }

    return id;
}

/* The type of a Future Boy! window with this Hugo rectangle on a screen of
   SCREENWIDTH x SCREENHEIGHT characters. The side of the main picture
   that a picture type tells is kept in future_boy_graphics_alignment. */
static int heglk_future_boy_type(int left, int top, int right, int bottom, int screenwidth, int screenheight)
{
    struct fb_id id = heglk_id_future_boy_images(left, top, right, bottom, screenwidth, screenheight);
    if (id.alignment != FB_ALIGNMENT_NONE)
        future_boy_graphics_alignment = id.alignment;
    return id.type;
}

/* The same for window context I, on the screen it was set on */
static int heglk_future_boy_window_type(int i)
{
    return heglk_future_boy_type(wins[i].l, wins[i].t, wins[i].r, wins[i].b, wins[i].screenwidth_at_creation, wins[i].screenheight_at_creation);
}

/* Place Future Boy! window context I of TYPE in the layout for the current
   screen size, and draw its picture again. */
static void heglk_adjust_single_future_boy_image(int type, int i, int alignment) {

    int x0, y0, x1, y1;
    int upper_bound = rows_to_height(1);
    int lower_bound = (screenheight_in_chars / 2) * gcellh;
    int compass_height = gscreenh * FB_COMPASS_HEIGHT;

    int large_image_edge = gscreenw * FB_MAIN_PICTURE_WIDTH;

    x0 = 0;
    x1 = gscreenw;
    y0 = upper_bound;
    y1 = lower_bound;

    switch (type) {
        case FB_IMAGE_MAIN:
            x1 = large_image_edge;
            break;
        case FB_IMAGE_SUB_1:
            y1 = lower_bound - compass_height;
            x0 = large_image_edge + gcellw;
            x1 = x0 + gscreenw * FB_SUB_PICTURE_WIDTH;
            break;
        case FB_IMAGE_SUB_2:
            x0 = x1 - gscreenw * FB_SUB_PICTURE_WIDTH;
            y0 = upper_bound + compass_height;
            break;
        case FB_IMAGE_LINE:
            y0 = lower_bound;
            y1 = y0 + gcellh;
            wins[mainwin].y0 = y1;
            wins[mainwin].y1 = gscreenh;
            heglk_sizeifexists(mainwin);
            break;
        case FB_IMAGE_COMPASS:
            y1 = y0 + compass_height;
            x0 = x1 - gscreenw * FB_SUB_PICTURE_WIDTH;
            break;
        case FB_STATUS:
            y0 = 0;
            y1 = upper_bound;
            break;
        case FB_MAIN:
            y0 = lower_bound + gcellh;
            y1 = gscreenh;
            break;
        default:
            LOG("heglk_adjust_single_future_boy_image: could not identify image. returning.\n");
            return;
    }

    /* The layout above has the main picture on the left: mirror it */
    if (alignment == FB_ALIGNMENT_RIGHT) {
        int temp = x0;
        x0 = gscreenw - x1;
        x1 = gscreenw - temp;
    }

    wins[i].x0 = x0;
    wins[i].x1 = x1;
    wins[i].y0 = y0;
    wins[i].y1 = y1;
    wins[i].clear = 0;

    if (type == FB_STATUS || type == FB_MAIN)
        return;

    if (wins[i].filename == NULL) {
        LOG("Window %d (type %d) has no filename!\n", i, type);
        heglk_unmap_and_mark_clear(i);
        return;
    }

    win_loadimage(heglk_image_id(wins[i].offset, wins[i].length), wins[i].filename, wins[i].offset, wins[i].length);
    glui32 origwidth, origheight;
    win_sizeimage(&origwidth, &origheight);

    int xoff, yoff;
    heglk_fit_image(x1 - x0, y1 - y0, &origwidth, &origheight, &xoff, &yoff);

    heglk_unmap_window(i);
    wins[i].win = gli_new_window(wintype_Graphics, 0);

    heglk_sizeifexists(i);
    win_fillrect(wins[i].win->peer, heglk_color_for(i, wins[i].bg), 0, 0, abs(x1-x0), abs(y1-y0));
    win_drawimage(wins[i].win->peer, xoff, yoff, origwidth, origheight, 0, 0);
}

/* Future Boy!: lay out every window in use for the current screen size
   and draw its picture again. Called after an arrange event. */
static void heglk_adjust_future_boy_windows(void) {
    if (current_game != GAME_FUTURE_BOY)
        return;
    int type;
    for (int i = 1; i < nwins; i ++)
    {
        if (wins[i].clear)
            continue;
        type = heglk_future_boy_window_type(i);

        heglk_sizeifexists(i);
        heglk_adjust_single_future_boy_image(type, i, future_boy_graphics_alignment);
    }
}

/* Where hugo_settextwindow() puts a window: the Hugo rectangle (in
   characters, possibly adjusted), the Glk one (in pixels), and what the
   placement found out on the way. */
struct heglk_geometry {
    int left, top, right, bottom;
    int x0, y0, x1, y1;
    int new_below_status;
    int new_second_image_row;
    int halfscreenwidth;
    int peggedtoright;
};

/* A window along the top of the screen: the status line. */
static void heglk_place_status(struct heglk_geometry *g)
{
    g->bottom = heglk_adjust_pre_v24_status_when_setting_window(g->bottom);
    g->y1 = rows_to_height(g->bottom);
    if (!below_status && mainwin)
        below_status = mainwin;
    if (below_status)
    {
        wins[below_status].y0 = g->y1;
        if (wins[below_status].y1 < g->y1)
            wins[below_status].y1 = g->y1;
        if (below_status == mainwin)
        {
            wins[below_status].t = g->bottom + 1;
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
    g->right = screenwidth_in_chars + 2;
}

/* Any other window that does not reach the bottom of the screen. */
static void heglk_place_aux(struct heglk_geometry *g)
{
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

    g->x0 = (g->left - 1) * gcellw;
    g->y0 = (g->top - 1) * gcellh;
    g->x1 = g->right * gcellw;

    if (gscreenw - g->x1 < 3 * gcellw || g->right > screenwidth_in_chars - 1)
        g->x1 = gscreenw;

    if (ABS((g->right - g->left) - (screenwidth_in_chars / 2)) < 2 && current_game != GAME_FUTURE_BOY)
    {
        g->x1 = g->x0 + gscreenw / 2;
        g->halfscreenwidth = true;

        LOG("Detected that aux window is supposed to be half screen width. Stretching it out.\n");

        // Hack to keep black room description window from
        // being deleted at the first turn of Guilty Bastards (trace: guilty-*)
        if (current_game == GAME_GUILTY_BASTARDS && curwin)
        {
            if (g->left == 1)
            {
                guilty_bastards_graphics_win = curwin;
            }
            else
            {
                guilty_bastards_aux_win = curwin;
                hugo_mapcurwin();
                wins[curwin].clear = 0;
                g->x0 = gscreenw / 2;
                g->x1 = gscreenw;
                g->peggedtoright = 1;
            }
        }
    }

    if (ABS(g->right - screenwidth_in_chars) < 2 && !heglk_find_window_edge(EDGE_RIGHT, g->left))
    {
        int diff = gscreenw - g->x1;
        g->x1 = gscreenw;
        g->x0 += diff;
        g->peggedtoright = true;
    }

    /* A height of 1 and a height of 0 will both
     be considered 1 character high */
    g->y1 = g->y0 + gcellh * (g->bottom - g->top);
    if (g->y1 == g->y0)
        g->y1 += gcellh;

    if (statuswin && ABS(g->top - wins[statuswin].b) < 2)
    {
        g->new_below_status = true;
    } else if (!statuswin && g->top <= 2) {
        g->new_below_status = true;
        int diff = 2 * ggridmarginy + gcellh + gcellh * (screenwidth_in_chars < MAX_STATUS_CHARS) - g->y0;
        g->y0 += diff;
        g->y1 += diff;
    } else {
        int attop = heglk_find_window_edge(EDGE_BOTTOM, g->top);
        if (!second_image_row && attop && (attop == below_status || wins[attop].t == wins[below_status].t))
        {
            g->new_second_image_row = true;
        }
    }
}

/* The window that reaches the bottom: the main text window. */
static void heglk_place_main(struct heglk_geometry *g)
{
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
    g->y1 = gscreenh;
    if (statuswin && !wins[statuswin].clear && (below_status || second_image_row))
    {
        if (g->top == 1 && below_status && below_status != mainwin)
        {
                wins[below_status].clear = 1;
                below_status = mainwin;
        }
        int above_main = heglk_find_window_edge(EDGE_BOTTOM, g->top);

        if (!above_main) {
            above_main = heglk_find_window_edge(EDGE_BOTTOM, wins[mainwin].t);
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
            g->y0 = heglk_push_down_main_window_to(wins[above_main].y1);
        } else {
            g->y0 = heglk_push_down_main_window_to(wins[statuswin].y1);
        }
    }

    if (!statuswin && !below_status && !second_image_row)
    {
        g->new_below_status = true;
    }
}

void hugo_settextwindow(int left, int top, int right, int bottom)
{
    struct heglk_geometry g;
    int type = ISMAIN;
    int matches = 0;

    int i;

    wins[curwin].cury = currentline;    /* one-based */
    wins[curwin].curx = currentpos;     /* zero-based */

    LOG("hugo_settextwindow %d %d %d %d\n", left, top, right, bottom);

    if (bottom < top)
    {
        LOG("hugo_settextwindow: negative height. Reversing top and bottom.\n");
        SWAP_INT(top, bottom);
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
            matches++;
        }
    }

    if (matches > 1)
    {
        LOG("hugo_settextwindow: Found more than one match! (%d)\n", matches);
    }

    /* unmap old unused windows that are cleared */
    hugo_unmapcleared();

    // We assume that no status window is more than 20 chars high
    // This fixes Cryptozookeeper when starting with a small window
    if ((top != 1 || bottom > 20)
        /* Pre-v2.4 didn't support proper windowing */
        && (game_version >= 24 || !inwindow))
    {

        /* Glk-illegal floating window: it ends above the previous text
         * window, whose bottom physical_windowbottom still holds (set by
         * heglk_record_physical, or by the engine when it restores the
         * window after a window block).
         */
        if (bottom < physical_windowbottom/FIXEDLINEHEIGHT+1)
        {
            type = ISAUX;
        }
        else {
            // type ISMAIN is the default
            if (left == 1 && top == 1 && right >= HUGO_INFINITE && bottom >= HUGO_INFINITE)
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
    }

    g = (struct heglk_geometry) {
        .left = left, .top = top, .right = right, .bottom = bottom,
        .x0 = 0, .y0 = 0, .x1 = gscreenw, .y1 = rows_to_height(bottom),
    };

    if (type == ISSTATUS)
        heglk_place_status(&g);
    else if (type == ISAUX)
        heglk_place_aux(&g);
    else
        heglk_place_main(&g);

    if (!curwin)
    {
        /* ... create a new window context if no suitable exists... */
        curwin = heglk_new_wincontext();
        LOG("Created new window context %d\n", curwin);
        heglk_init_wincontext(curwin, type);

        if (type == ISMAIN)
            mainwin = curwin;
        else if (type == ISSTATUS)
            statuswin = curwin;
    }

    wins[curwin].isaux = (type == ISAUX);
    wins[curwin].peggedtoright = g.peggedtoright;
    wins[curwin].halfscreenwidth = g.halfscreenwidth;

    if (g.new_below_status)
    {
        below_status = curwin;
        if (statuswin)
        {
            g.y0 = wins[statuswin].y1;
        }
    }

    if (g.new_second_image_row)
    {
        second_image_row = curwin;
        g.y1 = heglk_push_down_main_window_to(g.y1);
    }

    if (g.y1 > gscreenh)
        g.y1 = gscreenh;
    if (g.x1 > gscreenw)
        g.x1 = gscreenw;

    // If we reuse the context,
    // most of these will have the right value already...

    wins[curwin].l = g.left;
    wins[curwin].t = g.top;
    wins[curwin].r = g.right;
    wins[curwin].b = g.bottom;
    wins[curwin].x0 = g.x0;
    wins[curwin].y0 = g.y0;
    wins[curwin].x1 = g.x1;
    wins[curwin].y1 = g.y1;
    wins[curwin].screenwidth_at_creation = screenwidth_in_chars;
    wins[curwin].screenheight_at_creation = screenheight_in_chars;


    heglk_adjust_guilty_bastards_windows();

    heglk_record_physical(&wins[curwin]);
}

/* A buffer window has no cursor to move, so a move in it means the game is
 * about to redraw the window: discard it (after a pause if it has unread
 * text), except in the cases below. */
static void heglk_settextpos_in_buffer(void)
{
    // Hack to keep black room description window from
    // being deleted at the first turn of Guilty Bastards (trace: guilty-*)
    if (is_gb_room_description_window(curwin))
    {
        LOG(" move in buffer window, Guilty bastards.\n");
        return;
    }

    // Haunting the Ghosts: the first move after the author photo puts
    // the main window below the photo instead.
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

    if (current_game == GAME_FUTURE_BOY)
        return;

    LOG("  move in buffer window. unmap.\n");
    if (!wins[curwin].isaux)
        heglk_pause_before_discard(curwin);
    heglk_unmap_window(curwin);
}

void hugo_settextpos(int x, int y)
{

    /* The top-left corner of the current active window is (1, 1). */

    currentline = y;

    if (y == 2 && wins[curwin].clear)
        currentline = 1;

    currentpos = (x-1) * CHARWIDTH;   /* Note:  zero-based */

    if (wins[curwin].win == NULL)
    {
        return;
    }

    if (y >= HUGO_INFINITE)
    {
        // Moving off-screen discards a grid or graphics window. Future Boy!
        // gets a fresh one at once.
        if ((wins[curwin].win->type == wintype_TextGrid && !menuwin) || wins[curwin].win->type == wintype_Graphics) {
            LOG("  moved to line %d in grid window. unmap.\n", y);
            int remap = (current_game == GAME_FUTURE_BOY && wins[curwin].win->type == wintype_Graphics);
            heglk_unmap_window(curwin);
            if (remap)
                hugo_mapcurwin();
        }
        return;
    }
    else if (wins[curwin].win->type == wintype_TextBuffer)
    {
        heglk_settextpos_in_buffer();
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

/* A text window to the right of the left margin, opened by the game with
 * "window" while not in monospace mode: a side panel of game text. */
static int heglk_is_right_aux_window(void)
{
    return !mono && inwindow && wins[curwin].l > 5;
}

/* Whether wins[curwin] should become a text buffer rather than a grid. */
static int heglk_wants_buffer_window(void)
{
    int is_main_text_window = (curwin == mainwin && (currentline > HUGO_INFINITE || !mono));

    // A one-line menu header below the status line. Haunting the Ghosts
    // and World Builder use a fixed-width style for the regular game text,
    // so the usual check for a fixed-width font doesn't work; make the
    // menu a buffer to get proper word wrapping.
    int is_menu_header = (currentline == 1 && statuswin && curwin != statuswin);
    // Hack (trace: htgessay-*)
    int is_htg_menu = (current_game == GAME_HTG && is_menu_header && wins[curwin].fg == 7 && wins[curwin].bg == 0);
    // Hack (trace: worldbuilder-*)
    int is_worldbuilder_menu = (current_game == GAME_WORLDBUILDER && is_menu_header && !menuwin);

    return is_main_text_window || is_htg_menu || is_worldbuilder_menu || heglk_is_right_aux_window();
}

/* Check if the current "context" wins[curwin] has a corresponding Glk Window object. If not, create one */
static void hugo_mapcurwin(void)
{

    if (!curwin || wins[curwin].win != NULL)
        return; // The current window context already has a corresponding Glk window

    if (heglk_wants_buffer_window())
    {
        LOG(" * map win to buffer %d\n", curwin);

        /* We try to change to input style text color to something readable, */
        /* i.e. different from the background color */
        if (wins[curwin].bg != DEF_BGCOLOR && wins[curwin].fg != DEF_FCOLOR)
            glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, hugo_color(wins[curwin].fg));
        else
            glk_stylehint_clear(wintype_TextBuffer, style_Input, stylehint_TextColor);

        // Make input readable on the opening screen of Will The Real Marjorie Hopkirk Please Stand Up?
        if (current_game == GAME_MARJORIE && wins[curwin].fg == DEF_FCOLOR && gfgcol == 0 && wins[curwin].bg == 0)
        {
            glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, hugo_color(HUGO_WHITE));
        }

        wins[curwin].win = gli_new_window(wintype_TextBuffer, 0);
        if (current_game == GAME_CZK && heglk_is_right_aux_window() && wins[mainwin].y0 > wins[curwin].y1)
        {
                wins[curwin].y1 = wins[mainwin].y0;
        }

    }
    else
    {
        LOG(" * map win to grid %d\n", curwin);

        wins[curwin].win = gli_new_window(wintype_TextGrid, 0);

        int has_graphics_window = 0;
        // If the main window is a grid, we guess that this is a menu
        if (curwin == mainwin && curwin != statuswin)
            inmenu = true;

        if (curwin == mainwin)
        {
            for (int i = 1; i < nwins; i++) {
                if (wins[i].win && wins[i].win->type == wintype_Graphics)
                    has_graphics_window = 1;
            }
        }

        // Make all grid windows in Tetris transparent
        if (current_game == GAME_TETRIS) {
            win_maketransparent(wins[curwin].win->peer);
        }

        if (has_graphics_window)
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
    wins[curwin].colors_sent = false;
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

static void hugo_flushnl(void)
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

static void hugo_printnl(void)
{
    if (wins[curwin].win && wins[curwin].win->type == wintype_TextBuffer)
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
/* heglk_adjust_pre_v24_status_when_setting_window() above. */

static void heglk_adjust_pre_v24_status_when_printing(void)
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
        // Hack to make Colossal Cave status bar grow when needed: on a
        // narrow screen its status line takes two lines (trace: colossal-size-*)
        (screenwidth_in_chars < MAX_STATUS_CHARS && !inmenu);
    }

    // Hack to make Colossal Cave status bar shrink back to one line
    // when the screen is wide again or a menu is shown
    if (lowest_printed_position_in_status > 1 && (screenwidth_in_chars > MAX_STATUS_CHARS || inmenu))
        lowest_printed_position_in_status = 1;
}

/* Send the current window the zcolors for its colours */
static void heglk_zcolor(void)
{
    if (!wins[curwin].win)
        return;
    glk_set_window(wins[curwin].win);

    struct heglk_colorctx ctx = heglk_colorctx_of(curwin);

    /* At the top of a window whose background is back at its own default,
       hand the paper back to the theme too */
    int own_default_bg = (ctx.wintype == wintype_TextBuffer && ctx.bg == DEF_BGCOLOR)
                      || (ctx.wintype == wintype_TextGrid && ctx.bg == DEF_SLBGCOLOR);
    if (own_default_bg && ((currentpos == 0 && currentline == 1) || wins[curwin].clear))
    {
        win_setbgnd(wins[curwin].win->peer, -1);
        LOG("Trying to set window background back to default\n");
    }

    struct heglk_zcolors z = heglk_zcolors_of(&ctx);
    garglk_set_zcolors(z.fg, z.bg);
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
        if (current_game == GAME_FUTURE_BOY && (a[0] == ' ' || a[0] == '\r' || a[0] == '\n'))
            return;
        LOG("  unmap graphics window %d for printing\n", curwin);
        heglk_pause_before_discard(curwin);
        heglk_unmap_window(curwin);
    }

    /* check if we are trying to print a menu in the main buffer window */
    // Enceladus
    if (curwin == mainwin && mono && a[0] == '[' && wins[mainwin].clear && !inwindow)
    {
        hugo_settextpos(1, 1);
    }

    hugo_mapcurwin();

    glk_set_window(wins[curwin].win);

    glk_set_style(curstyle);

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

    /* The engine ends a line with "\r", then "\n" to scroll (hemisc.c).
       Glk scrolls by itself, so a lone "\n" prints nothing. */
    if (a[0] == '\r')
    {
        hugo_printnl();
    }
    else if (a[0] != '\n')
    {
        if (!wins[curwin].colors_sent
            || wins[curwin].fg != wins[curwin].sentfg
            || wins[curwin].bg != wins[curwin].sentbg)
        {
            heglk_zcolor();
        }

        hugo_flushnl();

        wins[curwin].colors_sent = true;
        wins[curwin].sentfg = wins[curwin].fg;
        wins[curwin].sentbg = wins[curwin].bg;

        if (current_game == GAME_TETRIS && a[0] == 'B' && mono)
        {
            win_moveto(wins[curwin].win->peer, currentpos, currentline - 2);
            glk_put_string("Score:");
            hugo_settextpos(currentpos + 1, currentline);
        }
        glk_put_string(a);

    }

    just_displayed_something = true;
    waited_since_display = false;
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
        if (heglk_game_ignores_fixed_font_in_buffer() && wins[curwin].win && wins[curwin].win->type == wintype_TextBuffer)
        {
            return;
        }
        curstyle = style_Preformatted;
        mono = true;

        // Hack to avoid extra line breaks in the boxed intro text of
        // Captain Speedo (trace: speedo-*); not specific to that game
        if (inwindow && wins[curwin].l > 2 && wins[curwin].t > 2 && wins[curwin].win && wins[curwin].win->type == wintype_TextGrid)
        {
            glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);
        }
    } else mono = false;
}

int hugo_hasgraphics(void)
{
    return glk_gestalt(gestalt_Graphics, 0) && glk_gestalt(gestalt_DrawImage, wintype_TextBuffer);
}


/* A Future Boy! picture of WIDTH x HEIGHT is being drawn in the current
   window: recognise it by its size, note which side of the screen the
   text goes and where the line under the pictures is, and take down the
   windows showing an earlier picture of the same kind. */
static void heglk_note_future_boy_picture(int width, int height) {
    if (current_game != GAME_FUTURE_BOY)
        return;
    int type = heglk_id_future_boy_images(wins[curwin].l, wins[curwin].t, wins[curwin].r, wins[curwin].b, screenwidth_in_chars, screenheight_in_chars).type;
    if (width > 400 && width < 450 && height > 250 && height < 300) {
        // Main image
        if (wins[curwin].l < 3)
            future_boy_graphics_alignment = FB_ALIGNMENT_LEFT;
        else
            future_boy_graphics_alignment = FB_ALIGNMENT_RIGHT;
        type = FB_IMAGE_MAIN;
    } else if (width > 170 && width < 180 && height > 180 && height < 190) {
        // Sub image: the window's place tells which one
    } else if (width > 120 && width < 140 && height > 50 && height < 70) {
        // Compass image
        if (wins[curwin].l < 3)
            future_boy_graphics_alignment = FB_ALIGNMENT_RIGHT;
        else
            future_boy_graphics_alignment = FB_ALIGNMENT_LEFT;
        type = FB_IMAGE_COMPASS;
    } else if (width > 1020 && width < 1100 && height > 5 && height < 10) {
        // Line image
        future_boy_line = curwin;
        type = FB_IMAGE_LINE;
    } else {
        LOG("Future Boy unknown image kind: width:%d height:%d\n", width, height);
    }
    if (FB_IS_IMAGE(type)) {
        heglk_clear_future_boy_type(type, curwin);
    }
}

/* Remember in context I the picture just loaded, so that it can be drawn
   again when the window is rebuilt (see heglk_adjust_single_future_boy_image). */
static void heglk_remember_picture(int i, const char *filename, size_t offset, size_t length)
{
    heglk_clear_filename(i);
    wins[i].filename = strdup(filename);
    wins[i].offset = offset;
    wins[i].length = length;
}

/* Where the main window's text is going: the pixel x of its cursor, if
   it is a buffer window, or else 0. */
static int heglk_main_text_x(void)
{
    if (!mainwin)
        return 0;
    struct winctx *main = &wins[mainwin];
    if (!main->win || main->win->type != wintype_TextBuffer)
        return 0;
    LOG("  main x=%d y=%d\n", main->curx, main->cury);
    return main->x0 + main->curx * gcellw;
}

/* Per-game notes taken from a picture's resource length, before it is
   placed. */
static void heglk_note_picture_quirks(long reslength)
{
    if (current_game == GAME_HTG && (reslength == 3501 || reslength == 4117))
        showing_author_photo = true;

    showing_futureboy_title = (current_game == GAME_FUTURE_BOY && reslength == FB_TITLE_RESLEN);

    // Hack to clear old picture in A Crimson Spring after restore
    // (trace: scourge-*)
    if (current_game == GAME_CRIMSON) {
        for (int i = 1; i < nwins; i++) {
            if (wins[i].win && wins[i].win->type == wintype_Graphics) {
                wins[i].clear = 1;
            }
        }
    }

    if (reslength == FB_SECOND_ROW_RESLEN && current_game == GAME_FUTURE_BOY)
    {
        // Without this, everything breaks when restoring Future boy
        second_image_row = curwin;
    }
}

/* The pixel rectangle for a picture in the current window: the window's
   columns, its rows down to the second image row, stretched to the right
   edge or to half the screen as the window is. */
static void heglk_picture_rect(int *px0, int *py0, int *px1, int *py1)
{
    struct winctx *ctx = &wins[curwin];
    int x0 = (ctx->l - 1) * gcellw;
    int y0 = ctx->y0;
    int winwidth = (ctx->r - ctx->l + 1) * gcellw + 1;
    int x1 = x0 + winwidth;
    if (x1 > gscreenw)
        x1 = gscreenw;
    int y1 = ctx->y1;

    if (second_image_row && second_image_row != curwin && wins[second_image_row].y0 < y1)
    {
        y1 = wins[second_image_row].y0;
    }

    if (gscreenw - x1 < (gcellw * 2 + 2 * ggridmarginx) || ctx->peggedtoright || x1 > gscreenw)
    {
        if (x0 < ggridmarginx)
        {
            x0 = 0;
        } else if (ctx->halfscreenwidth)
        {
            x0 = gscreenw / 2;
        } else {
            int diff = gscreenw - x1;
            x0 += diff;
        }
        x1 = gscreenw;
    }

    if (ctx->halfscreenwidth && x0 < ggridmarginx)
    {
        x0 = 0;
        x1 = gscreenw / 2 + 1;
    }

    // Hack to center the title mugshot gallery in Guilty Bastards
    // (trace: guilty-*)
    if (current_game == GAME_GUILTY_BASTARDS && ctx->r - ctx->l < screenwidth_in_chars / 4) {
        int diff = ggridmarginx + gcellw;
        x0 += diff;
        x1 += diff;
    }

    if (y1 == y0) {
        y1 = y0 + (ctx->b - ctx->t) * gcellh;
        if (y1 > gscreenh)
        {
            y1 = gscreenh;
            y0 = gscreenh - gcellh;
        }
    }

    LOG("heglk_picture_rect curwin (%d) x0: %d y0: %d x1: %d y1: %d \n", curwin, x0, y0, x1, y1);

    *px0 = x0;
    *py0 = y0;
    *px1 = x1;
    *py1 = y1;
}

/* Should the picture go inline in the main window, rather than in a
   graphics window of its own? It does when the window Hugo set for it
   starts where the main window's text is going (MAIN_TEXT_X) and ends
   where the main window does: the game is putting it in the text. */
static int heglk_should_draw_inline(int main_text_x)
{
    if (!mainwin || !wins[mainwin].win || !heglk_game_allows_inline_pictures())
        return false;
    if (current_game == GAME_TRAVELING)
        return true;

    LOG("ABS(main_text_x - wins[curwin].x0) = %d  gbufcellw * 2 = %f\n", ABS(main_text_x - wins[curwin].x0), gbufcellw * 2);
    LOG("ABS(wins[mainwin].y1 - wins[curwin].y1) = %d  gbufcellh * 2 = %f\n", ABS(wins[mainwin].y1 - wins[curwin].y1), gbufcellh * 2);

    // The cursor position is inaccurate, as it is counted in grid cells
    // while the main window is a buffer window, so allow two characters
    // either way.
    return ABS(main_text_x - wins[curwin].x0) < gbufcellw * 2
        && ABS(wins[mainwin].y1 - wins[curwin].y1) < gbufcellh * 2;
}

/* Draw the picture (ORIGWIDTH wide unscaled, WIDTH x HEIGHT as fitted to
   the current window) centred in a paragraph of its own in the main
   window, scaled to fit inside the buffer margins, and drop the current
   window, which it replaces. */
static void heglk_draw_inline_picture(glui32 origwidth, float aspect, glui32 width, glui32 height)
{
    int x0 = wins[mainwin].x0 + gbuffermarginx;
    int x1 = wins[mainwin].x1 - gbuffermarginx;

    if (origwidth < x1 - x0) {
        width = origwidth;
        height = origwidth * aspect;
    } else if (width > x1 - x0) {
        width = x1 - x0;
        height = width * aspect;
    }

    int linecount = height / gbufcellh + 1;
    LOG("  PictureInText at line %d, window %d\n", linecount, mainwin);

    nlbufcnt -= linecount;
    if (nlbufcnt < 0)
        nlbufcnt = 0;

    hugo_flushnl();
    glk_set_window(wins[mainwin].win);

    glk_set_style(style_User1);
    glk_put_char(' ');
    glk_set_hyperlink(1);
    win_drawimage(wins[mainwin].win->peer, imagealign_InlineUp, 0, width, height, 0, 0);
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

/* Replace the current window by a graphics window at X0,Y0-X1,Y1, filled
   with its background colour, with the picture drawn WIDTH x HEIGHT at
   XOFF,YOFF in it. */
static void heglk_draw_picture_window(int x0, int y0, int x1, int y1, int xoff, int yoff, glui32 width, glui32 height)
{
    if (wins[curwin].win)
    {
        LOG("  unmap old window %d for graphics\n", curwin);
        if (wins[curwin].win->type != wintype_Graphics)
            heglk_pause_before_discard(curwin);
        heglk_unmap_window(curwin);
    }

    LOG("  heglk_draw_picture_window %d: x0:%d y0:%d x1:%d y1:%d\n", curwin, x0 , y0, x1, y1);
    wins[curwin].x0 = x0;
    wins[curwin].y0 = y0;
    wins[curwin].x1 = x1;
    wins[curwin].y1 = y1;

    wins[curwin].win = gli_new_window(wintype_Graphics, 0);
    win_sizewin(wins[curwin].win->peer, x0, y0, x1, y1);

    int color = hugo_color(wins[curwin].bg);

    /* Avoid clear background behind images in Guilty Bastards */
    /* Can happen after changing settings */
    if (current_game == GAME_GUILTY_BASTARDS)
        glk_window_set_background_color(wins[curwin].win, color);
    else
        win_maketransparent(wins[curwin].win->peer);

    LOG("heglk_draw_picture_window: fill a rect with background color %x: width %d, height %d\n", color, abs(x1-x0), abs(y1-y0));
    win_fillrect(wins[curwin].win->peer, color, 0, 0, abs(x1-x0), abs(y1-y0));
    LOG("heglk_draw_picture_window: draw image: x offset %d, y offset %d, width %d, height %d\n", xoff, yoff, width, height);
    win_drawimage(wins[curwin].win->peer, xoff, yoff, width, height, 0, 0);
    wins[curwin].clear = 0;
}

int hugo_displaypicture(HUGO_FILE infile, long reslength)
{
    glui32 width, height;

    if (!hugo_hasgraphics())
    {
        glk_stream_close(infile, NULL);
        just_displayed_something = true;
        waited_since_display = false;
        return true;
    }

    int main_text_x = heglk_main_text_x();

    if (current_game == GAME_FUTURE_BOY)
        below_status = 0;

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

    int file_offset = glk_stream_get_position(infile);
    win_loadimage(heglk_image_id(file_offset, reslength), infile->filename, file_offset, reslength);
    win_sizeimage(&width, &height);
    LOG("hugo_displaypicture: original image size: width %d height %d , reslength: %ld\n", width, height, reslength);

    if (width < 1 || height < 1)
    {
        glk_stream_close(infile, NULL);
        return false;
    }

    heglk_remember_picture(curwin, infile->filename, file_offset, reslength);
    heglk_note_picture_quirks(reslength);

    glui32 origwidth = width;
    glui32 origheight = height;
    float aspect = (float) height / width; /* for the inline case */

    int x0, y0, x1, y1;
    heglk_picture_rect(&x0, &y0, &x1, &y1);

    int xoff, yoff;
    heglk_fit_image(x1 - x0, y1 - y0, &width, &height, &xoff, &yoff);

    heglk_note_future_boy_picture(origwidth, origheight);

    LOG("hugo_displaypicture w=%d h=%d x offset: %d y offset: %d\n", width, height, xoff, yoff);

    hugo_unmapcleared();

    // An inline picture would break graphical menus that expect mouse
    // input at the right coordinates, such as the main menu of
    // Cryptozookeeper; heglk_game_allows_inline_pictures() keeps those out.
    if (heglk_should_draw_inline(main_text_x))
        heglk_draw_inline_picture(origwidth, aspect, width, height);
    else
        heglk_draw_picture_window(x0, y0, x1, y1, xoff, yoff, width, height);

    glk_stream_close(infile, NULL);
    just_displayed_something = true;
    waited_since_display = false;
    return true;
}

void my_srandom(int seed)
{
    /* Seed the shared erkyrath_random() generator.  Under the determinism
       testing theme always use the fixed seed 1234; otherwise honor Hugo's
       requested seed (NORMALIZE_RANDOM passes 1 for a repeatable sequence,
       INIT_RANDOM / startup pass a time-based seed). */
    if (gli_determinism) {
        set_erkyrath_random(1234);
    } else {
        set_erkyrath_random(seed);
    }
}

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

#define LOG(...) // fprintf(stderr, __VA_ARGS__)

#define ABS(a) ((a) < 0 ? -(a) : (a))

#include <stdio.h>

#define MAXWINS 256

enum { PNONE, PGRID, PBUFFER, PGRAPHICS };

struct winctx
{
    int l, t, b, r;	    /* hugo coords */
    int x0, y0, x1, y1;	    /* real, mapped, coords */
    winid_t win;	    /* mapped peer windows */
    int clear;		    /* any printed content? */
    int ismain;
    int curx, cury;	    /* last cursor position */
};

static struct winctx wins[MAXWINS];
static int nwins = 0;
static int curwin = 0;
static int curstyle = style_Normal;
static int glk_fgcolor = 0;
static int glk_bgcolor = 0;

static int istetris = false;
static int keypress = 0;

static int nlbufcnt;
static int nlbufwin;

#define CHARWIDTH 1

static void hugo_mapcurwin(void);
static void hugo_handlearrange(void);
static void hugo_flushnl(void);


#define PIC 0
#define SND 1
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
int hugo_convert_key(int n) {

    switch (n)
    {
        case keycode_Left:    n = 8;    break;
        case keycode_Right:    n = 21;    break;
        case keycode_Up:    n = 11;    break;
        case keycode_Down:    n = 10;    break;
        case keycode_Return:    n = 13;    break;
        case keycode_Escape:    n = 27;    break;
    }

    return n;
}

/* hugo_timewait
 * Waits for 1/n seconds.  Returns false if waiting is unsupported.
 */

int hugo_timewait(int n)
{
    glui32 millisecs;
    event_t ev;

    if (!glk_gestalt(gestalt_Timer, 0))
	return false;

    if (n == 0)
	return true;

    millisecs = 1000 / n;
    if (millisecs == 0)
	millisecs = 1;

    if (istetris && keypress == 0 && wins[curwin].win->char_request == false)
        glk_request_char_event(wins[curwin].win);

    glk_request_timer_events(millisecs);
    while (1)
    {
	glk_select(&ev);
	if (ev.type == evtype_Arrange)
	    hugo_handlearrange();
    if (istetris && ev.type == evtype_CharInput) {
        keypress = hugo_convert_key(ev.val1);
    }
	if (ev.type == evtype_Timer)
	    break;
    }
    glk_request_timer_events(0);

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
    garglk_set_story_title(t);
    if (!strncmp(t, "Hugo Tetris", 11))
        istetris = true;
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
    char *buf;
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
    buf = malloc(reslen);

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
    gli_set_sound_resource(id, type, buf, reslen);
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

#define STAT_UNAVAILABLE (-1)

int hugo_iskeywaiting(void)
{
    if (istetris && keypress) {
        return true;
    }
    var[system_status] = STAT_UNAVAILABLE;
    return 0;
}


void hugo_scrollwindowup()
{
    /* Glk should look after this for us */
}

int hugo_getkey(void)
{
    if (istetris) {
        int oldkeypress = keypress;
        keypress = 0;
        return oldkeypress;
    }
    /* Not needed here--single-character events are handled
	   solely by hugo_waitforkey(), below */
    return 0;
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
    /* Color-setting functions should always pass the color through
    hugo_color() in order to properly set default fore/background colors:
    */
    
    if (c==16)      c = DEF_FCOLOR;
    else if (c==17) c = DEF_BGCOLOR;
    else if (c==18) c = DEF_SLFCOLOR;
    else if (c==19) c = DEF_SLBGCOLOR;
    else if (c==20) c = hugo_color(fcolor);	/* match foreground */
    
    /* Uncomment this block of code and change "c = ..." values if the system
	palette differs from the Hugo palette.

	If colors are unavailable on the system in question, it may suffice
	to have black, white, and brightwhite (i.e. boldface).  It is expected
	that colored text will be visible on any other-colored background.
    */

    switch (c)
    {
	case DEF_FCOLOR:	 c = 0; break;
	case DEF_BGCOLOR:	 c = 0; break;
	case DEF_SLFCOLOR:	 c = 0; break;
	case DEF_SLBGCOLOR:	 c = 0; break;

	case 0:	c = 1;  break;
	case 1:	c = 5;  break;
	case 2:	c = 3;  break;
	case 3:	c = 7;  break;
	case 4:	c = 2;  break;
	case 5:	c = 6;  break;
	case 6:	c = 4;  break;
	case 7:	c = 8;  break;
	case 8:	c = 0;  break;
	case 9:	c = 5;  break;
	case 10: c = 3; break;
	case 11: c = 7; break;
	case 12: c = 2; break;
	case 13: c = 6; break;
	case 14: c = 4; break;
	case 15: c = 8; break;
    }

    return c;
}

void hugo_settextcolor(int c)
{
    glk_fgcolor = hugo_color(c);
}


void hugo_setbackcolor(int c)
{
    glk_bgcolor = hugo_color(c);
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
    glui32 width = (gscreenw - 2 * ggridmarginx) / gcellw;
    return width - 1; /* -1 to override automatic line wrapping */
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

    nwins = 0;
    curwin = -1;

    nlbufwin = -1;
    nlbufcnt = 0;
    
    /* create initial fullscreen window */
    hugo_settextwindow(1, 1,
		       SCREENWIDTH/FIXEDCHARWIDTH,
		       SCREENHEIGHT/FIXEDLINEHEIGHT);
}

void hugo_cleanup_screen(void)
{
    /* Does whatever has to be done to clean up the display pre-termination. */
}


/*
 * Get a line of input from the keyboard, storing it in <buffer>.
 */

void hugo_getline(char *prompt)
{
    event_t ev;

    //LOG("getline '%s'\n", prompt);

    /* make sure we have a window */
    hugo_mapcurwin();

    /* Print prompt */
    hugo_print(prompt);

    /* Request line input */
    glk_request_line_event(wins[curwin].win, buffer, MAXBUFFER, 0);

    while (1)
    {
	glk_select(&ev);
	if (ev.type == evtype_Arrange)
	    hugo_handlearrange();
	if (ev.type == evtype_LineInput)
	    break;
    }

    /* The line we have received in commandbuf is not null-terminated */
    buffer[ev.val1] = '\0';	/* i.e., the length */

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
    event_t ev;
    
    /* make sure we have a window */
    hugo_mapcurwin();
    
    glk_request_char_event(wins[curwin].win);
    
    while (1)
    {
	glk_select(&ev);
	if (ev.type == evtype_Arrange)
	    hugo_handlearrange();
	if (ev.type == evtype_CharInput)
	    break;
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

    nlbufwin = -1;
    nlbufcnt = 0;

    for (i = 0; i < nwins; i++)
    {
	//LOG(" + delete %d\n", i);
	if (wins[i].win)
	    gli_delete_window(wins[i].win);
    }
    nwins = 0;
    curwin = -1;

    /* create initial fullscreen window */
    hugo_settextwindow(1, 1,
		       SCREENWIDTH/FIXEDCHARWIDTH,
		       SCREENHEIGHT/FIXEDLINEHEIGHT);

    currentpos = 0;
    currentline = 1;
}

/* Clear the currently defined window, move the cursor to the top-left
 * corner of the window.
 */

void hugo_clearwindow(void)
{
    int l = wins[curwin].l;
    int t = wins[curwin].t;
    int r = wins[curwin].r;
    int b = wins[curwin].b;
    int i;

    LOG("hugo_clearwindow bg=%d\n", glk_bgcolor);

    if (curwin == nlbufwin)
    {
	nlbufwin = -1;
	nlbufcnt = 0;
    }

    for (i = 0; i < nwins; i++)
    {
	if (wins[i].l >= l && wins[i].r <= r && wins[i].t >= t && wins[i].b <= b)
	// if (wins[i].l < r && wins[i].r > l && wins[i].t < b && wins[i].b > t)
	{
	    if (wins[i].win)
	    {
		LOG("   cleared %d\n", i);
		glk_window_clear(wins[i].win);
		win_setbgnd(wins[i].win->peer, glk_bgcolor);
	    }
	    wins[i].clear = 1;
	    wins[i].curx = 0;
	    wins[i].cury = 0;
	}
    }

    currentpos = 0;
    currentline = 1;
}

static void hugo_unmapcleared(void)
{
    int i;

    for (i = 0; i < nwins; i++)
    {
	if (i != curwin && wins[i].clear && wins[i].win)
	{
	    LOG(" + unmap window %d\n", i);
	    gli_delete_window(wins[i].win);
	    wins[i].win = 0;
	}
    }
}    

void hugo_handlearrange(void)
{
//    int i;
//    LOG("arrange event ... zap aux wins!");
//    for (i = 0; i < nwins; i++)
//    {
//    if (i == curwin)
//        continue;
//    if (wins[i].ismain)
//        continue;
//    if (wins[i].l == 1 && wins[i].t == 1 && wins[i].r > 0x7000)
//        continue;
//    if (wins[i].win)
//    {
//        LOG(" + unmap window %d (zap)\n", i);
//        gli_delete_window(wins[i].win);
//        wins[i].win = 0;
//    }
//    }
}

/* Hugo's arbitrarily positioned windows don't currently
 * mesh with what Glk has to offer, so we have to delay any
 * non-Glk-ish Windows and just maintain the current
 * parameters for creating floating TextGrids later when they
 * are printed to.
 */

void hugo_settextwindow(int left, int top, int right, int bottom)
{
    int x0, y0, x1, y1;
    int ismain = 0;
    int i;

    if (curwin >= 0)
    {
        wins[curwin].cury = currentline;    /* one-based */
        wins[curwin].curx = currentpos;	    /* zero-based */
    }

    LOG("hugo_settextwindow %d %d %d %d\n", left, top, right, bottom);
    
    /* detect main window and set size and flag with top=0 */
    if ((left == 1 && right > 0x7000 && bottom > 0x7000) || !inwindow)
    {
	x0 = 0;
	x1 = gscreenw;
	if (top == 1)
	    y0 = 0;
	else
	    y0 = ggridmarginy * 2 + (top - 1) * gcellh;
	y1 = gscreenh;
	ismain = 1;
    }

    /* find a match */
    curwin = -1;
    for (i = 0; i < nwins; i++)
    {
	if ((wins[i].ismain && ismain) ||
	    (wins[i].l == left && wins[i].t == top &&
	     wins[i].r == right && wins[i].b == bottom))
	{
	    curwin = i;
	}
    }

    /* unmap old unused windows that are cleared */
    hugo_unmapcleared();

    if (curwin != -1)
    {
	//LOG("  reuse ctx %d %s\n", curwin, ismain ? "(main)" : "");
	
	/* we may need to resize the main window */
	if (ismain)
	{
	    wins[curwin].t = top;
	    wins[curwin].x0 = x0;
	    wins[curwin].y0 = y0;
	    wins[curwin].x1 = x1;
	    wins[curwin].y1 = y1;
	    if (wins[curwin].win)
	    {
		//LOG("    really [%d %d %d %d]\n", x0, y0, x1, y1);
		win_sizewin(wins[curwin].win->peer,
			    wins[curwin].x0, wins[curwin].y0,
			    wins[curwin].x1, wins[curwin].y1);
	    }
	}
    }
    
    else
    {
	//LOG("  create ctx %d [%d %d %d %d] ", nwins, left, top, right, bottom);

	curwin = nwins;
	wins[curwin].l = left;
	wins[curwin].t = top;
	wins[curwin].r = right;
	wins[curwin].b = bottom;
	wins[curwin].win = 0;
	wins[curwin].clear = 1;
	wins[curwin].ismain = ismain;
	
	if (ismain)
	{
	    /* we already set the size above */
	    //LOG("mainwin\n");
	}
	else if (left == 1 && top == 1 && right > 0x7000)
	{
	    x0 = 0;
	    y0 = 0;
	    x1 = gscreenw;
	    y1 = ggridmarginy * 2 + bottom * gcellh;
	    //LOG("statuswin\n");
	}
	else
	{
	    x0 = ggridmarginx + gcellw * (left - 1);
	    y0 = ggridmarginy * 2 + gcellh * (top - 1);
	    if (right > 0x7000)
		x1 = gscreenw - ggridmarginx;
	    else
		x1 = ggridmarginx + gcellw * right;
	    if (bottom > 0x7000)
		y1 = gscreenh - ggridmarginy;
	    else
		y1 = ggridmarginy * 2 + gcellh * bottom;
	    //LOG("auxwin\n");
	}
	
	wins[curwin].x0 = x0;
	wins[curwin].y0 = y0;
	wins[curwin].x1 = x1;
	wins[curwin].y1 = y1;

    	nwins ++;
    }
    
    /* record position of the window */
    physical_windowleft = (left-1)*FIXEDCHARWIDTH;
    physical_windowtop = (top-1)*FIXEDLINEHEIGHT;
    physical_windowright = right*FIXEDCHARWIDTH-1;
    physical_windowbottom = bottom*FIXEDLINEHEIGHT-1;
    physical_windowwidth = (right-left+1)*FIXEDCHARWIDTH;
    physical_windowheight = (bottom-top+1)*FIXEDLINEHEIGHT;
}


void hugo_settextpos(int x, int y)
{
   LOG("hugo_settextpos %d %d\n", x, y);

    /* The top-left corner of the current active window is (1, 1). */

    currentline = y;
    currentpos = (x-1) * CHARWIDTH;   /* Note:  zero-based */
    
    if (wins[curwin].win)
    {
	if (wins[curwin].win->type == wintype_TextBuffer)
	{
	    if (y < 0x7000)
	    {
		LOG("  move in buffer window. unmap.\n");
		if (!wins[curwin].clear)
		    hugo_waitforkey();
		gli_delete_window(wins[curwin].win);
		wins[curwin].win = 0;
	    }
	}
	else if (wins[curwin].win->type == wintype_TextGrid)
	{
	    glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);
	}
    }
}

static void hugo_mapcurwin()
{
    int mono = (curstyle == style_Preformatted);
    int quartz, i;

    if (wins[curwin].win == 0)
    {
	if (wins[curwin].ismain &&
	    (currentline > 0x7000 ||
	     (currentline == 1 && !mono) ||
	     (currentline >= heglk_get_screenheight() && !mono)))
	{
	    LOG(" * map win to buffer %d\n", curwin);
	    wins[curwin].win = gli_new_window(wintype_TextBuffer, 0);
	    win_setbgnd(wins[curwin].win->peer, glk_bgcolor);
	}
	else
	{
	    LOG(" * map win to grid %d\n", curwin);
	    wins[curwin].win = gli_new_window(wintype_TextGrid, 0);
	    win_setbgnd(wins[curwin].win->peer, glk_bgcolor);
	    if (wins[curwin].ismain || wins[curwin].t != 1)
	    {
		//LOG(" - is aux or main window\n");
		if (wins[curwin].ismain)
		{
		    //LOG(" - ismain, check transparency\n");
		    quartz = 0;
		    for (i = 0; i < nwins; i++)
			if (wins[i].win && wins[i].win->type == wintype_Graphics)
			    quartz = 1;
		}
		else
		{
		    quartz = 1;
		}
		
		if (quartz)
		    win_maketransparent(wins[curwin].win->peer);
	    }
		//LOG(" - is status window\n");
        if (wins[curwin].win->type == wintype_TextGrid)
            glk_window_move_cursor(wins[curwin].win, currentpos, currentline - 1);
	}
	win_sizewin(wins[curwin].win->peer,
		    wins[curwin].x0, wins[curwin].y0, 
		    wins[curwin].x1, wins[curwin].y1);
    }
}

void hugo_flushnl(void)
{
    if (nlbufcnt && nlbufwin >= 0)
    {
	glk_set_window(wins[nlbufwin].win);
	while (nlbufcnt--)
	    glk_put_char('\n');
	nlbufwin = -1;
	nlbufcnt = 0;
	glk_set_window(wins[curwin].win);
    }
}

void hugo_printnl(void)
{
    if (wins[curwin].win->type == wintype_TextBuffer)
    {
	if (nlbufwin != curwin)
	    hugo_flushnl();
	nlbufwin = curwin;
	nlbufcnt ++;
    }
    else
	glk_put_char('\n');
}

void hugo_print(char *a)
{
    /* Essentially the same as printf() without formatting, since printf()
    generally doesn't take into account color setting, font changes,
    windowing, etc.

    The newline character '\n' must be explicitly included at the end of
    a line in order to produce a linefeed.  The new cursor position is set
    to the end of this printed text.  Upon hitting the right edge of the
    screen, the printing position wraps to the start of the next line.
    */

    static char just_printed_linefeed = false;

    int mono = (curstyle == style_Preformatted);

    if (curwin == -1)
    {
	LOG("printing to hell\n");
	return;
    }

    hugo_unmapcleared();

    /* printing to a window that is showing graphics... unmap */
    if (wins[curwin].win && wins[curwin].win->type == wintype_Graphics)
    {
        LOG("  unmap graphics window %d for printing\n", curwin);
        if (!wins[curwin].clear)
            hugo_waitforkey();
        gli_delete_window(wins[curwin].win);
        wins[curwin].win = 0;
    }

    /* check if we need to unmap a cleared window so we can swap type */
    if (wins[curwin].win && wins[curwin].clear)
    {
        if (wins[curwin].win->type == wintype_TextGrid)
        {
            if (wins[curwin].ismain &&
                (currentline > 0x7000 ||
                 (currentline == 1 && !mono) ||
                 (currentline >= heglk_get_screenheight() && !mono)))
            {
                LOG("  unmap grid main window %d so we can swap back to buffer\n", curwin);
                if (!wins[curwin].clear)
                    hugo_waitforkey();
                gli_delete_window(wins[curwin].win);
                wins[curwin].win = 0;
            }
        }
    }

    hugo_mapcurwin();

    wins[curwin].clear = 0;

    glk_set_window(wins[curwin].win);
    glk_set_style(curstyle | (glk_fgcolor << 8) | (glk_bgcolor << 16));
    
    // LOG("hugo print: '%s' %c\n", a, mono ? 'm' : 'p');
    
    if (a[0]=='\n')
    {
	if (!just_printed_linefeed)
	{
	    hugo_printnl();
	}
	else
	    just_printed_linefeed = false;
    }
    else if (a[0]=='\r')
    {
	if (!just_printed_linefeed)
	{
	    hugo_printnl();
	    just_printed_linefeed = true;
	}
	else
	    just_printed_linefeed = false;
    }
    else
    {
	hugo_flushnl();
	glk_put_string(a);
	just_printed_linefeed = false;
    }
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
	curstyle = style_Preformatted;
    }
}

int hugo_hasgraphics(void)
{
    return glk_gestalt(gestalt_Graphics, 0) && glk_gestalt(gestalt_DrawImage, wintype_TextBuffer);
}

int hugo_displaypicture(HUGO_FILE infile, long reslength)
{
    glui32 width, height;
    int i, mainwin;
    int oldx = 0;
    int oldy = 0;
    mainwin = -1;

    for (i = 0; i < nwins; i++)
    {
	if (wins[i].ismain && wins[i].win && wins[i].win->type == wintype_TextBuffer)
	{
	    //LOG("  main x=%d y=%d\n", wins[i].curx, wins[i].cury);
	    oldx = wins[i].x0 + wins[i].curx * gcellw;
	    oldy = wins[i].y0 + (wins[i].cury-1) * gcellh;
	    if (oldy > wins[i].y1)
		oldy = wins[i].y1;
	    mainwin = i;
	}
    }

    if (hugo_hasgraphics())
    {
	char *buf = malloc(reslength);
	if (buf)
	{
	    glk_get_buffer_stream(infile, buf, reslength);
	    win_loadimage(0, buf, reslength);
	    free(buf);
	    
	    win_sizeimage(&width, &height);
	    
	    float aspect = (float) height / width;
	    
	    int x0 = wins[curwin].x0;
	    int y0 = wins[curwin].y0;
	    int x1 = wins[curwin].x1;
	    int y1 = wins[curwin].y1;
	    
	    if (width > x1 - x0) { width = x1 - x0; height = width * aspect; }
	    if (height > y1 - y0) { height = y1 - y0; width = height / aspect; }
	    
	    int xoff = ((x1 - x0) - width) / 2;
	    int yoff = ((y1 - y0) - height) / 2;
	    
	    LOG("hugo_displaypicture w=%d h=%d\n", width, height);
	    
	    hugo_unmapcleared();
	    
	    if (mainwin != -1 &&
		ABS(oldx - wins[curwin].x0) < gcellw * 2 &&
		ABS(wins[mainwin].y1 - wins[curwin].y1) < gcellh * 2)
	    {
		int linecount = height / gcellh + 1;
		LOG("  PictureInText %d\n", linecount);

		if (1) // nlbufwin == mainwin)
		{
		    nlbufcnt -= linecount;
		    if (nlbufcnt < 0)
			nlbufcnt = 0;
		}
		
		hugo_flushnl();
		
		glk_set_window(wins[mainwin].win);
		win_drawimage(wins[mainwin].win->peer, imagealign_InlineUp, 0, width, height);
		wins[mainwin].clear = 0;
		glk_put_char('\n');
	    }
	    else
	    {
		if (wins[curwin].win)
		{
		    LOG("  unmap old window %d for graphics\n", curwin);
		    if (wins[curwin].win->type != wintype_Graphics && !wins[curwin].clear)
			hugo_waitforkey();
		    gli_delete_window(wins[curwin].win);
		    wins[curwin].win = 0;	
		}
		
		wins[curwin].win = gli_new_window(wintype_Graphics, 0);
		win_sizewin(wins[curwin].win->peer,
			    wins[curwin].x0, wins[curwin].y0, 
			    wins[curwin].x1, wins[curwin].y1);
		win_maketransparent(wins[curwin].win->peer);
		win_fillrect(wins[curwin].win->peer, 0, 0, 0, x1-x0, y1-y0);
		win_drawimage(wins[curwin].win->peer, xoff, yoff, width, height);
		wins[curwin].clear = 0;
	    }
	}
    }

    glk_stream_close(infile, NULL);
    return true;
}


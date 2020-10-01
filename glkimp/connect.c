#include "glkimp.h"
#include "protocol.h"

#define PBUFSIZE (GLKBUFSIZE / sizeof(unsigned short))
#define RBUFSIZE (GLKBUFSIZE / sizeof(struct fillrect))

#define MIN(a,b) (a < b ? a : b)

#define MAXWIN 64

enum { BUFNONE, BUFPRINT, BUFRECT };

static struct message wmsg;
static char wbuf[GLKBUFSIZE];
static unsigned short *pbuf = (void*)wbuf;

/* These structs are used to transmit information that
  won't fit into the standard message struct */
static struct fillrect *rbuf = (void*)wbuf;
static struct sizewinrect *sizewin = (void*)wbuf;
static struct drawrect *drawstruct = (void*)wbuf;
static struct settings_struct *settings = (void*)wbuf;

int readfd = 0;
int sendfd = 1;

int buffering = BUFNONE;
int bufferwin = -1;
int bufferatt = -1;
int bufferlen = 0;

int gli_utf8output = TRUE;
int gli_utf8input = TRUE;

int gli_enable_graphics = 0;
int gli_enable_sound = 0;
int gli_enable_styles = 0;

int gscreenw = 1;
int gscreenh = 1;
int gbuffermarginx = 0;
int gbuffermarginy = 0;
int ggridmarginx = 0;
int ggridmarginy = 0;
float gcellw = 8;
float gcellh = 12;
float gbufcellw = 8;
float gbufcellh = 12;
float gleading = 0;

uint32_t gtimerinterval = 0;

uint32_t gfgcol = 0;
uint32_t gbgcol = 0;
uint32_t gsfgcol = 0;
uint32_t gsbgcol = 0;

glui32 lasteventtype = -1;

void sendmsg(int cmd, int a1, int a2, int a3, int a4, int a5, size_t len, char *buf)
{
    ssize_t n;
    struct message msgbuf;

    msgbuf.cmd = cmd;
    msgbuf.a1 = a1;
    msgbuf.a2 = a2;
    msgbuf.a3 = a3;
    msgbuf.a4 = a4;
    msgbuf.a5 = a5;
    msgbuf.len = len;

#ifdef DEBUG
        //fprintf(stderr, "SENDMSG %d len=%d\n", cmd, len);
#endif
    n = write(sendfd, &msgbuf, sizeof msgbuf);
    if (n != sizeof msgbuf)
    {
        fprintf(stderr, "protocol error. exiting.\n");
        exit(1);
    }

    if (len)
    {
        n = write(sendfd, buf, len);
        if (n != len)
        {
            fprintf(stderr, "protocol error. exiting.\n");
            exit(1);
        }
    }
}

void readmsg(struct message *msgbuf, char *buf)
{
    ssize_t n;

    n = read(readfd, msgbuf, sizeof (struct message));
    if (msgbuf->cmd == ERROR || n != sizeof (struct message))
    {
        fprintf(stderr, "protocol error. exiting.\n");
        exit(1);
    }

    if (msgbuf->len)
    {
        n = read(readfd, buf, msgbuf->len);
        if (n != msgbuf->len)
        {
            fprintf(stderr, "protocol error. exiting.\n");
            exit(1);
        }
    }

    buf[msgbuf->len] = 0;
}

void win_hello(void)
{
    event_t event;

    win_flush();

    sendmsg(HELLO, 0, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);

    gli_enable_graphics = wmsg.a1;
    gli_enable_sound = wmsg.a2;

    event.type = 0;

    // get first event, which should always be Arrange
    win_select(&event, 1);
    if (event.type != evtype_Arrange)
    {
        fprintf(stderr, "protocol handshake error\n");
        exit(1);
    }
}

void win_flush(void)
{
    if (buffering == BUFNONE)
        return;

    //	fprintf(stderr, "win_flush buf=%d len=%d win=%d\n", buffering, bufferlen, bufferwin);

    if (buffering == BUFPRINT)
    {
        sendmsg(PRINT, bufferwin, bufferatt, 0, 0, 0,
                bufferlen * sizeof(unsigned short),
                (char*)pbuf);
    }

    if (buffering == BUFRECT)
    {
        sendmsg(FILLRECT, bufferwin, bufferlen, 0, 0, 0,
                bufferlen * sizeof(struct fillrect),
                (char*)rbuf);
    }

    buffering = BUFNONE;
    bufferlen = 0;
    bufferatt = -1;
    bufferwin = -1;
}

void win_print(int name, int ch, int at)
{
    if (buffering == BUFRECT)
        win_flush();

    if (buffering == BUFPRINT && bufferwin != name)
        win_flush();
    if (buffering == BUFPRINT && bufferatt != at)
        win_flush();
    if (buffering == BUFPRINT && (unsigned long)bufferlen >= PBUFSIZE)
        win_flush();

    if (buffering == BUFNONE)
    {
        buffering = BUFPRINT;
        bufferwin = name;
        bufferatt = at;
        bufferlen = 0;
    }

    pbuf[bufferlen++] = ch;
}

/* Gargoyle glue */

void win_unprint(int name, glui32 *s, int len)
{
    if (!len)
        return;
    
    win_flush();

    glui32 ix;
    for (ix=0; ix<len; ix++) {
         pbuf[ix] = s[ix];
    }

    sendmsg(UNPRINT, name, 0, 0, 0, 0,
            len * sizeof(unsigned short),
            (char *)pbuf);
}

void wintitle(void)
{
    size_t len = strlen(gli_story_title);
    if (len) {
        char *buf = malloc(len + 1);
        strcpy(buf, gli_story_title);

        if (strlen(buf))
        {
            sendmsg(SETTITLE, 0, 0, 0, 0, 0,
                    (int)len,
                    buf);
        }
        free(buf);
    }
}

/* End of Gargoyle glue */

void win_fillrect(int name, glui32 color, int x, int y, int w, int h)
{
    if (buffering == BUFPRINT)
        win_flush();

    if (buffering == BUFRECT && bufferwin != name)
        win_flush();
    if (buffering == BUFRECT && (unsigned long)bufferlen >= RBUFSIZE)
        win_flush();

    if (buffering == BUFNONE)
    {
        buffering = BUFRECT;
        bufferwin = name;
        bufferlen = 0;
    }

    rbuf[bufferlen].color = color;
    rbuf[bufferlen].x = x;
    rbuf[bufferlen].y = y;
    rbuf[bufferlen].w = w;
    rbuf[bufferlen].h = h;
    bufferlen++;
}

char *win_promptopen(int type)
{
    win_flush();
    sendmsg(PROMPTOPEN, type, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);
    return wbuf;
}

char *win_promptsave(int type)
{
    win_flush();
    sendmsg(PROMPTSAVE, type, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);
    return wbuf;
}

int win_newwin(int type)
{
    int expected_peer;
    win_flush();

    if (type == wintype_Graphics && !gli_enable_graphics)
        return -1;

    /* We calculate which peer id this new window should get,
     i.e. the first integer (including 0) that is not used by a window already.
     We must tell the window server which peer we expect, because we may have
     autorestored and the window may have been created already in the window server
     process. */

    for (expected_peer = 0; expected_peer < MAXWIN; expected_peer++)
        if (gli_window_for_peer(expected_peer) == NULL)
            break;

    sendmsg(NEWWIN, type, expected_peer, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);

    if (wmsg.a1 != expected_peer)
        fprintf(stderr, "win_newwin: Error! Expected window with peer %d, got %d\n", expected_peer, wmsg.a1);
    
    return wmsg.a1;
}

void win_delwin(int name)
{
    win_flush();
    sendmsg(DELWIN, name, 0, 0, 0, 0, 0, NULL);
}

void win_sizewin(int name, int x0, int y0, int x1, int y1)
{
    win_flush();
    /* The window size may have changed before the message reaches the
     window server, so we send (what this interpreter process thinks is)
     the screen width and height. */

    if (x0 > x1) {
        fprintf(stderr, "win_sizewin: Error! Negative width: %d, x0:%d x1:%d. Reversing!\n", x1 - x0, x0, x1);
        int temp = x0;
        x0 = x1;
        x1 = temp;
    }

    if (y0 > y1) {
        fprintf(stderr, "win_sizewin: Error! Negative height: %d, y0:%d y1:%d. Reversing!\n", y1 - y0, y0, y1);
        int temp = y0;
        y0 = y1;
        y1 = temp;
    }

    if (x1 > gscreenw * 2) {
        fprintf(stderr, "win_sizewin: Error! x1 too big: %d Setting to gscreenw:%d!\n", x1, gscreenw);
        x1 = gscreenw;
    }

    if (y1 > gscreenh * 2) {
        fprintf(stderr, "win_sizewin: Error! y1 too big: %d Setting to gscreenh:%d!\n", y1, gscreenh);
        y1 = gscreenh;
    }

    sizewin->x0 = x0;
    sizewin->y0 = y0;
    sizewin->x1 = x1;
    sizewin->y1 = y1;
    sizewin->gamewidth = gscreenw;
    sizewin->gameheight = gscreenh;
    sendmsg(SIZWIN, name, 0, 0, 0, 0,
            sizeof(struct sizewinrect),
            (char*)sizewin);
}

void win_maketransparent(int name)
{
    win_flush();
    sendmsg(MAKETRANSPARENT, name, 0, 0, 0, 0, 0, NULL);
}

void win_clear(int name)
{
    win_flush();
    sendmsg(CLRWIN, name, 0, 0, 0, 0, 0, NULL);
}

void win_moveto(int name, int x, int y)
{
    win_flush();
    sendmsg(MOVETO, name, x, y, 0, 0, 0, NULL);
}

void win_beep(int type)
{
    win_flush();
    sendmsg(BEEP, type, 0, 0, 0, 0, 0, NULL);
}

void win_timer(int millisecs)
{
    win_flush();
    sendmsg(TIMER, millisecs, 0, 0, 0, 0, 0, NULL);
    gtimerinterval = millisecs;
}

void win_initchar(int name)
{
    win_flush();
    sendmsg(INITCHAR, name, 0, 0, 0, 0, 0, NULL);
}

void win_cancelchar(int name)
{
    win_flush();
    sendmsg(CANCELCHAR, name, 0, 0, 0, 0, 0, NULL);
}

void win_initline(int name, int cap, int len, void *buf)
{
    win_flush();

    window_t *win = gli_window_for_peer(name);

    glui32 ix;

    if (win->line_request_uni) {
        for (ix=0; ix<len; ix++) {
            pbuf[ix] = ((glui32 *)buf)[ix];
        }
    } else {
        // If this was not a unicode line event request,
        // we convert to unicode here
        for (ix=0; ix<len; ix++) {
            pbuf[ix] = ((char *)buf)[ix];
            if ( pbuf[ix] >= 0x100)
                pbuf[ix] = '?';
        }
    }

    sendmsg(INITLINE, name, cap, 0, 0, 0, len * sizeof(unsigned short), (char *)pbuf);
}

void win_cancelline(int name, int cap, int *len, char *buf)
{
    win_flush();
    sendmsg(CANCELLINE, name, cap, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, buf);
    *len = wmsg.len / sizeof(unsigned short);
}

void win_setlink(int name, int val)
{
    win_flush();
    sendmsg(SETLINK, name, val, 0, 0, 0, 0, NULL);
}

void win_initlink(int name)
{
    win_flush();
    sendmsg(INITLINK, name, 0, 0, 0, 0, 0, NULL);
}

void win_cancellink(int name)
{
    win_flush();
    sendmsg(CANCELLINK, name, 0, 0, 0, 0, 0, NULL);
}

void win_set_echo(int name, int val)
{
    win_flush();
    sendmsg(SETECHO, name, val, 0, 0, 0, 0, NULL);
}

void win_set_terminators(int name, glui32 *keycodes, int count)
{
#ifdef DEBUG
    //	fprintf(stderr, "sent TERMINATORS win: %u count:%u\n",name, count);
    //	for (int i=0; i < count; i++)
    //		fprintf(stderr, "keycode %d = %u\n", i, keycodes[i]);
#endif
    win_flush();
    sendmsg(TERMINATORS, name, count, 0, 0, 0, sizeof(glui32) * count, (char *)keycodes);
}

void win_initmouse(int name)
{
    win_flush();
    sendmsg(INITMOUSE, name, 0, 0, 0, 0, 0, NULL);
}

void win_cancelmouse(int name)
{
    win_flush();
    sendmsg(CANCELMOUSE, name, 0, 0, 0, 0, 0, NULL);
}

void win_flowbreak(int name)
{
    win_flush();
    sendmsg(FLOWBREAK, name, 0, 0, 0, 0, 0, NULL);
}

int win_findimage(int resno)
{
    win_flush();
    if (!gli_enable_graphics)
        return 0;
    sendmsg(FINDIMAGE, resno, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);
    return wmsg.a1;
}

void win_loadimage(int resno, char *filename, int offset, int reslen)
{
    win_flush();
    if (gli_enable_graphics)
    {
        int len = strlen(filename);
        if (len)
        {
            char *buf = malloc(len + 1);
            strcpy(buf, filename);
            sendmsg(LOADIMAGE, resno, offset, reslen, 0, 0, len, buf);
            free(buf);
        }
    }
}

void win_sizeimage(glui32 *width, glui32 *height)
{
    win_flush();
    if (gli_enable_graphics)
    {
        sendmsg(SIZEIMAGE, 0, 0, 0, 0, 0, 0, NULL);
        readmsg(&wmsg, wbuf);
        if (width) *width = wmsg.a1;
        if (height) *height = wmsg.a2;
    }
    else
    {
        if (width) *width = 1;
        if (height) *height = 1;
    }
}

void win_drawimage(int name, glui32 val1, glui32 val2, glui32 width, glui32 height)
{
    win_flush();
    if (gli_enable_graphics)
    {

        window_t *win = gli_window_for_peer(name);

        drawstruct->x = val1;
        drawstruct->y = val2;
        drawstruct->width = width;
        drawstruct->height = height;
        drawstruct->style = win->style;

        sendmsg(DRAWIMAGE, name, 0, 0, 0, 0,
                    sizeof(struct drawrect),
                    (char*)drawstruct);
    }
}

void win_stylehint(int wintype, int styl, int hint, int val)
{
    win_flush();
    sendmsg(STYLEHINT, wintype, styl, hint, val, 0, 0, NULL);
}

void win_clearhint(int wintype, int styl, int hint)
{
    win_flush();
    sendmsg(CLEARHINT, wintype, styl, hint, 0, 0, 0, NULL);
#ifdef DEBUG
    fprintf(stderr, "sent CLEARHINT type:%d styl:%d hint:%d\n",wintype, styl, hint);
#endif
}

int win_style_measure(int name, int styl, int hint, glui32 *result)
{
    win_flush();
    sendmsg(STYLEMEASURE, name, styl, hint, 0, 0, 0, NULL);
#ifdef DEBUG
    fprintf(stderr, "sent STYLEMEASURE name:%d styl:%d hint:%d\n",name, styl, hint);
#endif
    readmsg(&wmsg, wbuf);
    *result = wmsg.a2;
    return wmsg.a1;  /* TRUE or FALSE */
}

void win_setbgnd(int name, glui32 color)
{
    win_flush();
    sendmsg(SETBGND, name, (int)color, 0, 0, 0, 0, NULL);
}

void win_sound_notify(int snd, int notify)
{
#ifdef DEBUG
    fprintf(stderr, "sent EVTSOUND snd:%d notify:%d\n",snd, notify);
#endif
    win_flush();
    sendmsg(EVTSOUND, 0, snd, notify, 0, 0, 0, NULL);
}

void win_volume_notify(int notify)
{
    win_flush();
    sendmsg(EVTVOLUME, 0, 0, notify, 0, 0, 0, NULL);
}

void win_autosave(int hash)
{
    win_flush();
    sendmsg(AUTOSAVE, 0, hash, 0, 0, 0, 0, NULL);
}

void win_setzcolor(int name, glsi32 fg, glsi32 bg)
{
    win_flush();
    sendmsg(SETZCOLOR, name, fg, bg, 0, 0, 0, NULL);
}

void win_setreverse(int name, int reverse)
{
    win_flush();
    sendmsg(SETREVERSE, name, reverse, 0, 0, 0, 0, NULL);
}

void win_reset()
{
    win_flush();
    sendmsg(RESET, 0, 0, 0, 0, 0, 0, NULL);
}

void win_select(event_t *event, int block)
{
    int i;
    win_flush();

again:

    sendmsg(NEXTEVENT, block, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);

    switch (wmsg.cmd)
    {
        case OKAY:
#ifdef DEBUG
            fprintf(stderr, "win_select: no event...?!\n");
#endif
            break;

        case EVTPREFS:
            gli_enable_graphics = wmsg.a2;
            if (gli_enable_sound == 1 && wmsg.a3 == 0)
                gli_stop_all_sound_channels();
            gli_enable_sound = wmsg.a3;
            gli_enable_styles = wmsg.a4;
            goto again;

        case EVTARRANGE:
#ifdef DEBUG
            fprintf(stderr, "win_select: arrange event\n");
            fprintf(stderr, "gscreenw: %u gscreenh: %u\n", settings->screen_width, settings->screen_height);
#endif
            /* + 5 for default line fragment padding */
            if (gscreenw == settings->screen_width &&
                gscreenh == settings->screen_height &&
                gbuffermarginx == settings->buffer_margin_x + 5 &&  // line fragment padding
                gbuffermarginy == settings->buffer_margin_y &&
                ggridmarginx == settings->grid_margin_x + 5 &&  // line fragment padding
                ggridmarginy == settings->grid_margin_y &&
                gcellw == settings->cell_width &&
                gcellh == settings->cell_height &&
                gbufcellw == settings->buffer_cell_width &&
                gbufcellh == settings->buffer_cell_height &&
				gleading == settings->leading &&
                gfgcol == settings->buffer_foreground &&
                gbgcol == settings->buffer_background &&
                gsfgcol == settings->grid_foreground &&
                gsbgcol == settings->grid_background &&
                settings->force_arrange == 0)
                goto again;

            event->type = evtype_Arrange;

            gscreenw = settings->screen_width;
            gscreenh = settings->screen_height;
            gbuffermarginx = settings->buffer_margin_x + 5; // line fragment padding
            gbuffermarginy = settings->buffer_margin_y;
            ggridmarginx = settings->grid_margin_x + 5;  // line fragment padding
            ggridmarginy = settings->grid_margin_y;
            gcellw = settings->cell_width;
            gcellh = settings->cell_height;
            gbufcellw = settings->buffer_cell_width;
            gbufcellh = settings->buffer_cell_height;
            gleading = settings->leading;
            gfgcol = settings->buffer_foreground;
            gbgcol = settings->buffer_background;
            gsfgcol = settings->grid_foreground;
            gsbgcol = settings->grid_background;

            gli_windows_rearrange();
            break;

        case EVTLINE:
#ifdef DEBUG
            //fprintf(stderr, "line input event\n");
#endif

            event->type = evtype_LineInput;
            event->win = gli_window_for_peer(wmsg.a1);
            event->val2 = wmsg.a3;

            event->val1 = MIN(wmsg.a2, event->win->line.cap);
            unsigned short *ibuf = (unsigned short*)wbuf;

            if (event->win->line_request_uni)
            {
                glui32 *obuf = event->win->line.buf;
                for (i = 0; i < (int)event->val1; i++)
                    obuf[i] = ibuf[i];
                if (event->win->echostr)
                    gli_stream_echo_line_uni(event->win->echostr, event->win->line.buf, event->val1);
            }
            else
            {
                unsigned char *obuf = event->win->line.buf;
                for (i = 0; i < (int)event->val1; i++)
                    obuf[i] = ibuf[i] < 0x100 ? ibuf[i] : '?';
                if (event->win->echostr)
                    gli_stream_echo_line(event->win->echostr, event->win->line.buf, event->val1);
            }

            event->win->str->readcount += event->val1;
            
            if (gli_unregister_arr)
            {
                (*gli_unregister_arr)(event->win->line.buf, event->win->line.cap,
                                      event->win->line_request_uni ? "&+#!Iu" : "&+#!Cn",
                                      event->win->line.inarrayrock);
            }

            event->win->line.buf = NULL;
            event->win->line.len = 0;
            event->win->line.cap = 0;

            event->win->line_request = FALSE;
            event->win->line_request_uni = FALSE;

            break;

        case EVTKEY:
#ifdef DEBUG
            fprintf(stderr, "key input event for %d\n", wmsg.a1);
#endif
            event->type = evtype_CharInput;
            event->win = gli_window_for_peer(wmsg.a1);
            if (event->win == NULL)
                break;
            event->val1 = wmsg.a2;
            if (!event->win->char_request_uni)
            {
                unsigned chr =  (unsigned)event->val1;
                if (chr > 0xFF && !(chr <= keycode_Unknown && chr >= keycode_Func12))
                    event->val1 = '?';
            }
            event->win->char_request = FALSE;
            event->win->char_request_uni = FALSE;
            break;

        case EVTMOUSE:
#ifdef DEBUG
            fprintf(stderr, "mouse input event\n");
#endif
            event->type = evtype_MouseInput;
            event->win = gli_window_for_peer(wmsg.a1);
            event->val1 = wmsg.a2;
            event->val2 = wmsg.a3;
            event->win->mouse_request = FALSE;
            break;
        case EVTTIMER:
#ifdef DEBUG
            fprintf(stderr, "timer event\n");
#endif
            event->type = evtype_Timer;
            break;
        case EVTSOUND:
#ifdef DEBUG
            fprintf(stderr, "sound notification event\n");
#endif
            event->type = evtype_SoundNotify;
            event->val1 = wmsg.a2;
            event->val2 = wmsg.a3;
            break;
        case EVTHYPER:
#ifdef DEBUG
            fprintf(stderr, "hyperlink event with val1 = %d\n",  wmsg.a2);
#endif
            event->type = evtype_Hyperlink;
            event->win = gli_window_for_peer(wmsg.a1);
            if (event->win == NULL)
            {
                fprintf(stderr, "Error: window %d not found!\n",  wmsg.a1);
                break;
            }
            event->val1 = wmsg.a2;
            event->win->hyper_request = FALSE;
            break;

		case EVTVOLUME:
#ifdef DEBUG
            fprintf(stderr, "volume notification event");
#endif
            event->type = evtype_VolumeNotify;
            event->val2 = wmsg.a3;
            break;

        default:
#ifdef DEBUG
            fprintf(stderr, "unknown event type: %d\n", wmsg.cmd);
#endif
            break;
    }
    lasteventtype = (event ? event->type : evtype_None);
}


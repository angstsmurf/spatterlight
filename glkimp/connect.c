#include "glkimp.h"
#include "protocol.h"

#define PBUFSIZE (GLKBUFSIZE / sizeof(unsigned short))
#define RBUFSIZE (GLKBUFSIZE / sizeof(struct fillrect))

#define MIN(a,b) (a < b ? a : b)

enum { BUFNONE, BUFPRINT, BUFRECT };

static struct message wmsg;
static char wbuf[GLKBUFSIZE];
static unsigned short *pbuf = (void*)wbuf;
static struct fillrect *rbuf = (void*)wbuf;

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

int gscreenw = 1;
int gscreenh = 1;
int gbuffermarginx = 0;
int gbuffermarginy = 0;
int ggridmarginx = 0;
int ggridmarginy = 0;
float gcellw = 8;
float gcellh = 12;

void sendmsg(int cmd, int a1, int a2, int a3, int a4, int a5, int len, char *buf)
{
    ssize_t n;
    struct message msgbuf;

    msgbuf.cmd = cmd;
    msgbuf.a1 = a1;
    msgbuf.a2 = a2;
    msgbuf.a3 = a3;
    msgbuf.a4 = a4;
    msgbuf.a5 = a5;
    msgbuf.a6 = 0;
    msgbuf.len = len;

#ifdef DEBUG
    //    fprintf(stderr, "SENDMSG %d len=%d\n", cmd, len);
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
    if (buffering == BUFPRINT && bufferlen >= PBUFSIZE)
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

void wintitle(void)
{
    char buf[256];

    if (strlen(gli_story_title))
        sprintf(buf, "%s", gli_story_title);
    else if (strlen(gli_story_name))
        sprintf(buf, "%s", gli_story_name);
        //sprintf(buf, "%s - %s", gli_story_name, gli_program_name);
    else
        sprintf(buf, "%s", gli_program_name);
    if (strlen(buf))
        sendmsg(SETTITLE, 0, 0, 0, 0, 0,
                (int)(strlen(buf)), // * sizeof(unsigned short)
                (char*)buf);
#ifdef DEBUG
    //fprintf(stderr, "Sent change title request: length %d, title %s (Latin-1, not Unicode)\n", (int)(strlen(buf)), (char*)buf);
#endif
}

/* End of Gargoyle glue */

void win_fillrect(int name, glui32 color, int x, int y, int w, int h)
{
    if (buffering == BUFPRINT)
        win_flush();

    if (buffering == BUFRECT && bufferwin != name)
        win_flush();
    if (buffering == BUFRECT && bufferlen >= RBUFSIZE)
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
    win_flush();
    if (type == wintype_Graphics && !gli_enable_graphics)
        return -1;
    sendmsg(NEWWIN, type, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);
    return wmsg.a1;
}

void win_delwin(int name)
{
    win_flush();
    sendmsg(DELWIN, name, 0, 0, 0, 0, 0, NULL);
}

void win_sizewin(int name, int a, int b, int c, int d)
{
    win_flush();
    sendmsg(SIZWIN, name, a, b, c, d, 0, NULL);
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

void win_timer(int millisecs)
{
    win_flush();
    sendmsg(TIMER, millisecs, 0, 0, 0, 0, 0, NULL);
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

void win_initline(int name, int cap, int len, char *buf)
{
    win_flush();
    sendmsg(INITLINE, name, cap, 0, 0, 0, len, buf);
}

void win_cancelline(int name, int cap, int *len, char *buf)
{
    win_flush();
    sendmsg(CANCELLINE, name, cap, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, buf);
    *len = wmsg.len;
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

void win_loadimage(int resno, char *buf, int len)
{
    win_flush();
    if (gli_enable_graphics)
        sendmsg(LOADIMAGE, resno, 0, 0, 0, 0, len, buf);
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
        sendmsg(DRAWIMAGE, name, val1, val2, width, height, 0, NULL);
}

int win_newchan(void)
{
    win_flush();
    if (!gli_enable_sound)
        return 0;
    sendmsg(NEWCHAN, 0, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);
    return wmsg.a1;
}

void win_delchan(int chan)
{
    win_flush();
    sendmsg(DELCHAN, chan, 0, 0, 0, 0, 0, NULL);
}

int win_findsound(int resno)
{
    win_flush();
    if (!gli_enable_sound)
        return 0;
    sendmsg(FINDSOUND, resno, 0, 0, 0, 0, 0, NULL);
    readmsg(&wmsg, wbuf);
    return wmsg.a1;
}

void win_loadsound(int resno, char *buf, int len)
{
    win_flush();
    if (gli_enable_sound)
        sendmsg(LOADSOUND, resno, 0, 0, 0, 0, len, buf);
}

void win_setvolume(int chan, int volume)
{
    win_flush();
    sendmsg(SETVOLUME, chan, volume, 0, 0, 0, 0, NULL);
}

void win_playsound(int chan, int repeats, int notify)
{
    win_flush();
    if (gli_enable_sound)
        sendmsg(PLAYSOUND, chan, repeats, notify, 0, 0, 0, NULL);
}

void win_stopsound(int chan)
{
    win_flush();
    sendmsg(STOPSOUND, chan, 0, 0, 0, 0, 0, NULL);
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
    //fprintf(stderr, "sent CLEARHINT type:%d styl:%d hint:%d\n",wintype, styl, hint);
#endif
}

int win_style_measure(int name, int styl, int hint, glui32 *result)
{
    win_flush();
    sendmsg(STYLEMEASURE, name, styl, hint, 0, 0, 0, NULL);
#ifdef DEBUG
    //fprintf(stderr, "sent STYLEMEASURE name:%d styl:%d hint:%d\n",name, styl, hint);
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

//void win_sound_notify(glui32 snd, glui32 notify)
void win_sound_notify(int snd, int notify)
{
#ifdef DEBUG
 //   fprintf(stderr, "sent EVTSOUND snd:%d notify:%d\n",snd, notify);
#endif
    win_flush();
    sendmsg(EVTSOUND, 0, snd, notify, 0, 0, 0, NULL);
}

void win_volume_notify(int notify)
{
    win_flush();
    sendmsg(EVTVOLUME, 0, 0, notify, 0, 0, 0, NULL);
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
            fprintf(stderr, "no event...?!\n");
#endif
            break;

        case EVTPREFS:
            gli_enable_graphics = wmsg.a1;
            gli_enable_sound = wmsg.a2;
            goto again;

        case EVTARRANGE:
#ifdef DEBUG
            //	     fprintf(stderr, "arrange event\n");
#endif
            if ( gscreenw == wmsg.a1 &&
                gscreenh == wmsg.a2 &&
                gbuffermarginx == wmsg.a3 &&
                gbuffermarginy == wmsg.a3 &&
                ggridmarginx == wmsg.a4 &&
                ggridmarginy == wmsg.a4 &&
                gcellw == wmsg.a5 / 256.0 &&
                gcellh == wmsg.a6 / 256.0 )
                goto again;

            event->type = evtype_Arrange;
            gscreenw = wmsg.a1;
            gscreenh = wmsg.a2;
            gbuffermarginx = wmsg.a3;
            gbuffermarginy = wmsg.a3;
            ggridmarginx = wmsg.a4;
            ggridmarginy = wmsg.a4;
            gcellw = wmsg.a5 / 256.0;
            gcellh = wmsg.a6 / 256.0;
            gli_windows_rearrange();
            break;

        case EVTLINE:
#ifdef DEBUG
            //fprintf(stderr, "line input event\n");

            //	     fprintf(stderr, "line input event\n");
#endif

            event->type = evtype_LineInput;
            event->win = gli_window_for_peer(wmsg.a1);

            if (event->win->line_request_uni)
            {
                event->val1 = MIN(wmsg.a2, event->win->line.cap); /* / sizeof(glui32));*/
                glui32 *obuf = event->win->line.buf;
                unsigned short *ibuf = (unsigned short*)wbuf;
                for (i = 0; i < event->val1; i++)
                    obuf[i] = ibuf[i];
                if (event->win->echostr)
                    gli_stream_echo_line_uni(event->win->echostr, event->win->line.buf, event->val1);
            }
            else
            {
                event->val1 = MIN(wmsg.a2, event->win->line.cap);
                unsigned char *obuf = event->win->line.buf;
                unsigned short *ibuf = (unsigned short*)wbuf;
                for (i = 0; i < event->val1; i++)
                    obuf[i] = ibuf[i] < 0x100 ? ibuf[i] : '?';
                if (event->win->echostr)
                    gli_stream_echo_line(event->win->echostr, event->win->line.buf, event->val1);
            }

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
            //fprintf(stderr, "key input event for %d\n", wmsg.a1);
#endif
            event->type = evtype_CharInput;
            event->win = gli_window_for_peer(wmsg.a1);
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
            //fprintf(stderr, "mouse input event\n");
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
}


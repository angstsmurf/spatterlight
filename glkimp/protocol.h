#define GLKBUFSIZE (1024 * 64)

struct message
{
    int cmd;
    int a1, a2, a3, a4, a5, a6;
    int len;
};

struct fillrect
{
    uint32_t color;
    short x, y, w, h;
};

void sendmsg(int msg, int a1, int a2, int a3, int a4, int a5, int len, char *buf);
void readmsg(struct message *msgbuf, char *buf);

enum
{
    NOREPLY,
    
    OKAY,
    ERROR,
    
    HELLO,
    
    PROMPTOPEN,
    PROMPTSAVE,
    NEWWIN,
    DELWIN,
    SIZWIN,
    CLRWIN,
    MOVETO,
    PRINT,
    MAKETRANSPARENT,
    STYLEHINT,
    CLEARHINT,
    STYLEMEASURE,
    SETBGND,
    SETTITLE,
    
    TIMER,
    INITCHAR,
    CANCELCHAR,
    INITLINE,
    CANCELLINE,
    SETECHO,
    INITMOUSE,
    CANCELMOUSE,
    
    FILLRECT,
    FINDIMAGE,
    LOADIMAGE,
    SIZEIMAGE,
    DRAWIMAGE,
    FLOWBREAK,
    
    NEWCHAN,
    DELCHAN,
    FINDSOUND,
    LOADSOUND,
    SETVOLUME,
    PLAYSOUND,
    STOPSOUND,

#ifdef GLK_MODULE_HYPERLINKS

    SETLINK,
    INITLINK,
    CANCELLINK,

#endif
    
    NEXTEVENT,
    EVTARRANGE,
    EVTLINE,
    EVTKEY,
    EVTMOUSE,
    EVTTIMER,
    EVTSOUND,
    EVTHYPER,
    
    EVTPREFS
};

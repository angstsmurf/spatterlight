#define GLKBUFSIZE (1024 * 64)

struct message
{
    int cmd;
    int a1, a2, a3, a4, a5;
    size_t len;
};

struct fillrect
{
    uint32_t color;
    short x, y, w, h;
};

struct sizewinrect
{
    glui32 x0, y0, x1, y1, gamewidth, gameheight;
};

struct drawrect
{
    glsi32 x, y;
    glui32 width, height, style;
};

struct settings_struct
{
    int screen_width;
    int screen_height;
    int buffer_margin_x;
    int buffer_margin_y;
    int grid_margin_x;
    int grid_margin_y;
    float cell_width;
    float cell_height;
    float buffer_cell_width;
    float buffer_cell_height;
    int buffer_foreground;
    int buffer_background;
    int grid_foreground;
    int grid_background;
    int do_styles;
    int quote_boxes;
    int sa_delays;
    int sa_display_style;
    int sa_inventory;
    int sa_palette;
    int slowdraw;
    int determinism;
    int error_handling;
    int force_arrange;
};

void sendmsg_glk(int msg, int a1, int a2, int a3, int a4, int a5, size_t len, char *buf);
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
    UNPRINT,
    MAKETRANSPARENT,
    STYLEHINT,
    CLEARHINT,
    STYLEMEASURE,
    SETBGND,
    SETTITLE,
    AUTOSAVE,
    RESET,
    BANNERCOLS,
    BANNERLINES,

    TIMER,
    INITCHAR,
    CANCELCHAR,
    INITLINE,
    CANCELLINE,
    SETECHO,
	TERMINATORS,
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
    PAUSE,
    UNPAUSE,

    BEEP,
    
    SETLINK,
    INITLINK,
    CANCELLINK,

    SETZCOLOR,
    SETREVERSE,

    QUOTEBOX,
    SHOWERROR,
    
    NEXTEVENT,
    EVTARRANGE,
    EVTREDRAW,
    EVTLINE,
    EVTKEY,
    EVTMOUSE,
    EVTTIMER,
    EVTHYPER,
    EVTSOUND,
	EVTVOLUME,

    EVTPREFS,
    EVTQUIT
};

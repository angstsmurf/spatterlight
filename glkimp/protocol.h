#ifndef GLKIMP_PROTOCOL
#define GLKIMP_PROTOCOL

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
    glsi32 x0, y0, x1, y1, gamewidth, gameheight;
};

struct drawrect
{
    glsi32 x, y;
    glui32 width, height, style;
    /* Glk 0.7.6 (GLK_MODULE_IMAGE2) scaling rule. 0 = legacy draw call
       (glk_image_draw / glk_image_draw_scaled): width/height are final
       pixel sizes and the pre-0.7.6 behaviour is kept. Nonzero = a
       glk_image_draw_scaled_ext() imagerule; width/height are then that
       rule's raw arguments and maxwidth is the fixed-point ($10000 = 1.0)
       window-width bound, resolved (and re-resolved on resize) app-side. */
    glui32 imagerule, maxwidth;
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
    int flicker;
    int zmachine_terp;
    int zmachine_no_err_win;
    int voiceover_on;
    int z6_graphics;
    int z6_colorize;
    int determinism;
    int error_handling;
    int comprehend_graphics;
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
    REFRESH,
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
    CANPRINT,
    PURGEIMG,
    MENUITEM,

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
    EVTQUIT,
    EVTTEST
};

#endif

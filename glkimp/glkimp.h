#ifndef GLKINT_H
#define GLKINT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "glk.h"
#include "gi_dispa.h"
#include "gi_blorb.h"
#include "glkstart.h"

#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

extern int gli_enable_graphics;
extern int gli_enable_sound;

extern int gscreenw;
extern int gscreenh;
extern int gbuffermarginx;
extern int gbuffermarginy;
extern int ggridmarginx;
extern int ggridmarginy;
extern float gcellw;
extern float gcellh;

/* more Gargoyle glue */
extern char gli_program_name[256];
extern char gli_program_info[256];
extern char gli_story_name[256];
extern char gli_story_title[256];
void wintitle(void);

/* window server connection */

void win_hello(void);
void win_bye(char *reason);
char *win_promptopen(int type);
char *win_promptsave(int type);

int win_newwin(int type);
void win_delwin(int name);
void win_sizewin(int name, int a, int b, int c, int d);
void win_maketransparent(int name);

void win_initchar(int name);
void win_cancelchar(int name);
void win_initline(int name, int cap, int len, char *buf);
void win_cancelline(int name, int cap, int *len, char *buf);
void win_setlink(int name, int val);
void win_initlink(int name);
void win_cancellink(int name);
void win_set_echo(int name, int val);
void win_set_terminators(int name, glui32 *keycodes, int count);
void win_initmouse(int name);
void win_cancelmouse(int name);

void win_setbgnd(int name, glui32 color);
void win_clear(int name);
void win_moveto(int name, int x, int y);
void win_timer(int millisecs);
void win_select(event_t *event, int block);
void win_flush(void);
void win_print(int name, int ch, int at);

void win_fillrect(int name, glui32 color, int left, int top, int width, int height);
void win_flowbreak(int name);
int  win_findimage(int resno);
void win_loadimage(int resno, char *buf, int len);
void win_sizeimage(glui32 *width, glui32 *height);
void win_drawimage(int name, glui32 val1, glui32 val2, glui32 width, glui32 height);

void win_stylehint(int type, int styl, int hint, int val);
void win_clearhint(int type, int styl, int hint);
int win_style_measure(int name, int styl, int hint, glui32 *result);

int win_newchan(void);
void win_delchan(int chan);
void win_setvolume(int chan, int volume);
int  win_findsound(int resno);
void win_loadsound(int resno, char *buf, int len);
void win_playsound(int chan, int repeats, int notify);
void win_stopsound(int chan);
void win_sound_notify(int snd, int notify);
void win_volume_notify(int notify);

/* unicode case mapping */

typedef glui32 gli_case_block_t[2]; /* upper, lower */
/* If both are 0xFFFFFFFF, you have to look at the special-case table */

typedef glui32 gli_case_special_t[3]; /* upper, lower, title */
/* Each of these points to a subarray of the unigen_special_array
(in cgunicode.c). In that subarray, element zero is the length,
and that's followed by length unicode values. */

typedef glui32 gli_decomp_block_t[2]; /* count, position */
/* The position points to a subarray of the unigen_decomp_array.
 If the count is zero, there is no decomposition. */

void gli_putchar_utf8(glui32 val, FILE *fl);
glui32 gli_getchar_utf8(FILE *fl);
glui32 gli_parse_utf8(unsigned char *buf, glui32 buflen, glui32 *out, glui32 outlen);

/* blorb resources */

int giblorb_is_resource_map(void);
void giblorb_get_resource(glui32 usage, glui32 resnum, FILE **file, long *pos, long *len, glui32 *type);

/* Callbacks for the dispatch layer */

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock);
extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock);


/* This macro is called whenever the library code catches an error
 or illegal operation from the game program. */

#define gli_strict_warning(msg) do { fprintf(stderr, "glk: %s\n", msg); } while (0)

/* The overall screen size, as set by command-line options. A
 better implementation would check the real screen size
 somehow. */
extern int gli_screenwidth, gli_screenheight;
/* Should we assume that the terminal (or whatever is handling our
 stdin/stdout) is expecting UTF-8 encoding? Normally input and output
 will be the same, but they don't have to be. */
extern int gli_utf8output, gli_utf8input;

#if GIDEBUG_LIBRARY_SUPPORT
/* Has the user requested debug support? */
extern int gli_debugger;
#else /* GIDEBUG_LIBRARY_SUPPORT */
#define gli_debugger (0)
#endif /* GIDEBUG_LIBRARY_SUPPORT */


typedef struct glk_window_struct window_t;
typedef struct glk_stream_struct stream_t;
typedef struct glk_fileref_struct fileref_t;
typedef struct glk_schannel_struct channel_t;

typedef struct grect_struct
{
    int x0, y0;
    int x1, y1;
} grect_t;

#define MAGIC_WINDOW_NUM (9876)
#define MAGIC_STREAM_NUM (8769)
#define MAGIC_FILEREF_NUM (7698)


typedef struct attr_s
{
    unsigned fgset   : 1;
    unsigned bgset   : 1;
    unsigned reverse : 1;
    unsigned         : 1;
    unsigned style   : 4;
    unsigned fgcolor : 24;
    unsigned bgcolor : 24;
    unsigned hyper   : 32;
} attr_t;

struct glk_window_struct {

    glui32 magicnum;
    glui32 rock;
    glui32 type;

    window_t *parent;		/* pair window which contains this one */
    grect_t bbox;		/* content rectangle, excluding borders */


    int peer;			/* GUI server peer window */

    struct
    {
        window_t *child1, *child2;
        glui32 dir;			/* winmethod_Left, Right, Above, or Below */
        int vertical, backward;		/* flags */
        glui32 division;		/* winmethod_Fixed or winmethod_Proportional */
        window_t *key;			/* NULL or a leaf-descendant (not a Pair) */
        glui32 size;			/* size value */
    } pair;

    struct
    {
        void *buf;
        int len;
        int cap;
        gidispatch_rock_t inarrayrock;
    } line;

    glui32 background;	/* for graphics windows */

    stream_t *str; /* the window stream. */
    stream_t *echostr; /* the window's echo stream, if any. */

    int line_request;
    int line_request_uni;
    int char_request;
    int char_request_uni;
    int mouse_request;
    int hyper_request;
    int more_request;
    int scroll_request;
    int image_loaded;

    glui32 echo_line_input;
    glui32 *line_terminators;
    glui32 termct;

    glui32 style;

    //void *linebuf;
    //glui32 linebuflen;
    //int linecap;
    //gidispatch_rock_t inarrayrock;

    attr_t attr;

    gidispatch_rock_t disprock;
    window_t *next, *prev; /* in the big linked list of windows */

};

extern char gli_workdir[];
extern char gli_workfile[];

/* Global but internal functions */

stream_t *gli_stream_open_window(window_t *win);
void gli_delete_stream(stream_t *str);
void gli_stream_fill_result(stream_t *str, stream_result_t *result);
void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);
void gli_windows_unechostream(stream_t *str);
void gli_window_put_char(window_t *win, unsigned ch);
void gli_windows_rearrange(void);
int gli_window_check_terminator(glui32 ch);
void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2);

window_t *gli_window_for_peer(int peer);


/* to be used by hugo for its standalone windows, no parent/pair hierarchy */
void gli_delete_window(window_t *win);



#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)
#define strtype_Resource (4)


struct glk_stream_struct {
    glui32 magicnum;
    glui32 rock;

    int type; /* file, window, or memory stream */
    int unicode; /* one-byte or four-byte chars? Not meaningful for windows */

    glui32 readcount, writecount;
    int readable, writable;

    /* for strtype_Window */
    window_t *win;

    /* for strtype_File */
    FILE *file;
    glui32 lastop; /* 0, filemode_Write, or filemode_Read */
    //int textfile;

    /* for strtype_Resource */
    int isbinary;

    /* for strtype_Memory and strtype_Resource. Separate pointers for
       one-byte and four-byte streams */
    unsigned char *buf;
    unsigned char *bufptr;
    unsigned char *bufend;
    unsigned char *bufeof;
    glui32 *ubuf;
    glui32 *ubufptr;
    glui32 *ubufend;
    glui32 *ubufeof;
    glui32 buflen;
    gidispatch_rock_t arrayrock;

    gidispatch_rock_t disprock;
    stream_t *next, *prev; /* in the big linked list of streams */
};

struct glk_fileref_struct {
    glui32 magicnum;
    glui32 rock;

    char *filename;
    int filetype;
    int textmode;

    gidispatch_rock_t disprock;
    fileref_t *next, *prev; /* in the big linked list of filerefs */
};

/* Declarations of library internal functions. */

extern void gli_initialize_misc(void);

extern window_t *gli_new_window(glui32 type, glui32 rock); /* does not touch hierarchy */
//extern window_t *gli_new_window(glui32 rock);


extern void gli_delete_window(window_t *win);
extern window_t *gli_window_get(void);

extern stream_t *gli_new_stream(int type, int readable, int writable,
                                glui32 rock);
extern void gli_delete_stream(stream_t *str);
//extern strid_t gli_stream_open_pathname(char *pathname, int textmode, glui32 rock);
extern strid_t gli_stream_open_pathname(char *pathname, int writemode,
                        int textmode, glui32 rock);
extern void gli_stream_set_current(stream_t *str);
extern void gli_stream_fill_result(stream_t *str,
                                   stream_result_t *result);
extern void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
extern void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);

extern fileref_t *gli_new_fileref(char *filename, glui32 usage,
                                  glui32 rock);
extern void gli_delete_fileref(fileref_t *fref);

extern void gli_putchar_utf8(glui32 val, FILE *fl);
extern int gli_encode_utf8(glui32 val, char *buf, int len);
extern glui32 gli_parse_utf8(unsigned char *buf, glui32 buflen,
                             glui32 *out, glui32 outlen);



enum { CHANNEL_IDLE, CHANNEL_SOUND, CHANNEL_MUSIC };

struct glk_schannel_struct
{
    glui32 rock;

    void *sample; /* Mix_Chunk (or FMOD Sound) */
    void *music; /* Mix_Music (or FMOD Music) */
    void *decode; /* Sound_Sample */

    void *sdl_rwops; /* SDL_RWops */
    unsigned char *sdl_memory;
    int sdl_channel;

    int resid; /* for notifies */
    int status;
    int channel;
    int volume;
    glui32 loop;
    int notify;
    int buffered;
    int paused;

    /* for volume fades */
    int volume_notify;
    int volume_timeout;
    int target_volume;
    double float_volume;
    double volume_delta;
    void *timer;

    gidispatch_rock_t disprock;
    channel_t *chain_next, *chain_prev;
};

extern void gli_initialize_sound(void);

/* A macro that I can't think of anywhere else to put it. */

#define gli_event_clearevent(evp)  \
((evp)->type = evtype_None,    \
(evp)->win = NULL,    \
(evp)->val1 = 0,   \
(evp)->val2 = 0)

/* A macro which reads and decodes one character of UTF-8. Needs no
 explanation, I'm sure.

 Oh, okay. The character will be written to *chptr (so pass in "&ch",
 where ch is a glui32 variable). eofcond should be a condition to
 evaluate end-of-stream -- true if no more characters are readable.
 nextch is a function which reads the next character; this is invoked
 exactly as many times as necessary.

 val0, val1, val2, val3 should be glui32 scratch variables. The macro
 needs these. Just define them, you don't need to pay attention to them
 otherwise.

 The macro itself evaluates to true if ch was successfully set, or
 false if something went wrong. (Not enough characters, or an
 invalid byte sequence.)

 This is not the worst macro I've ever written, but I forget what the
 other one was.
 */

#define UTF8_DECODE_INLINE(chptr, eofcond, nextch, val0, val1, val2, val3)  ( \
    (eofcond ? 0 : ( \
        (((val0=nextch) < 0x80) ? (*chptr=val0, 1) : ( \
            (eofcond ? 0 : ( \
                (((val1=nextch) & 0xC0) != 0x80) ? 0 : ( \
                    (((val0 & 0xE0) == 0xC0) ? (*chptr=((val0 & 0x1F) << 6) | (val1 & 0x3F), 1) : ( \
                        (eofcond ? 0 : ( \
                            (((val2=nextch) & 0xC0) != 0x80) ? 0 : ( \
                                (((val0 & 0xF0) == 0xE0) ? (*chptr=(((val0 & 0xF)<<12)  & 0x0000F000) | (((val1 & 0x3F)<<6) & 0x00000FC0) | (((val2 & 0x3F))    & 0x0000003F), 1) : ( \
                                    (((val0 & 0xF0) != 0xF0 || eofcond) ? 0 : (\
                                        (((val3=nextch) & 0xC0) != 0x80) ? 0 : (*chptr=(((val0 & 0x7)<<18)   & 0x1C0000) | (((val1 & 0x3F)<<12) & 0x03F000) | (((val2 & 0x3F)<<6)  & 0x000FC0) | (((val3 & 0x3F))     & 0x00003F), 1) \
                                        )) \
                                    )) \
                                )) \
                            )) \
                        )) \
                )) \
            )) \
        )) \
    )

#endif /* GLKINT_H */

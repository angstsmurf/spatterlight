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

#define OS_UNIX

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

/* window server connection */

void win_hello();
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
void win_initmouse(int name);
void win_cancelmouse(int name);

void win_setbgnd(int name, int color);
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

int win_newchan(void);
void win_delchan(int chan);
void win_setvolume(int chan, int volume);
int  win_findsound(int resno);
void win_loadsound(int resno, char *buf, int len);
void win_playsound(int chan, int repeats, int notify);
void win_stopsound(int chan);

/* unicode case mapping */

typedef glui32 gli_case_block_t[2]; /* upper, lower */
/* If both are 0xFFFFFFFF, you have to look at the special-case table */

typedef glui32 gli_case_special_t[3]; /* upper, lower, title */
/* Each of these points to a subarray of the unigen_special_array
(in cgunicode.c). In that subarray, element zero is the length,
and that's followed by length unicode values. */

void gli_putchar_utf8(glui32 val, FILE *fl);
glui32 gli_getchar_utf8(FILE *fl);
glui32 gli_parse_utf8(unsigned char *buf, glui32 buflen, glui32 *out, glui32 outlen);

/* blorb resources */

int giblorb_is_resource_map();
void giblorb_get_resource(glui32 usage, glui32 resnum, FILE **file, long *pos, long *len, glui32 *type);

/* Callbacks for the dispatch layer */

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock);
extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock);


#define gli_strict_warning(msg) do { fprintf(stderr, "glk: %s\n", msg); } while (0)

#define gli_event_clearevent(evp)  \
    ((evp)->type = evtype_None,    \
    (evp)->win = NULL,    \
    (evp)->val1 = 0,   \
    (evp)->val2 = 0)

typedef struct glk_window_struct window_t;
typedef struct glk_stream_struct stream_t;
typedef struct glk_fileref_struct fileref_t;
typedef struct glk_schannel_struct channel_t;

typedef struct grect_struct
{
    int x0, y0;
    int x1, y1;
} grect_t;

/* Global but internal functions */
stream_t *gli_stream_open_pathname(char *pathname, int textmode, glui32 rock);
stream_t *gli_stream_open_window(window_t *win);
void gli_delete_stream(stream_t *str);
void gli_stream_fill_result(stream_t *str, stream_result_t *result);
void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);
void gli_windows_unechostream(stream_t *str);
void gli_window_put_char(window_t *win, unsigned ch);
void gli_windows_rearrange(void);
window_t *gli_window_for_peer(int peer);

/* to be used by hugo for its standalone windows, no parent/pair hierarchy */
window_t *gli_new_window(glui32 type, glui32 rock); /* does not touch hierarchy */
void gli_delete_window(window_t *win);

struct glk_fileref_struct
{
    glui32 rock;
    char *filename;
    int filetype;
    int textmode;
    gidispatch_rock_t disprock;
    fileref_t *next, *prev; /* in the big linked list of filerefs */
};

#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)

struct glk_stream_struct
{
    glui32 rock;
    
    int type; /* file, window, or memory stream */
    int unicode; /* one-byte or four-byte chars? Not meaningful for windows. */
    
    glui32 readcount, writecount;
    int readable, writable;
    
    /* for strtype_Window */
    window_t *win;
    
    /* for strtype_Window */
    FILE *file;
    
    /* for strtype_Memory */
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

struct glk_window_struct
{
    glui32 rock;
    glui32 type;
    
    grect_t bbox;		/* content rectangle, excluding borders */
    window_t *parent;		/* pair window which contains this one */
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
    
    glui32 style;
    
    gidispatch_rock_t disprock;
    window_t *next, *prev; /* in the big linked list of windows */
};

struct glk_schannel_struct
{
    glui32 rock;
    int peer;
    gidispatch_rock_t disprock;
    channel_t *next, *prev;
};

#endif

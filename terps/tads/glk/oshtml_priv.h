/* oshtml_priv.h -- the HTML TADS parser's state, shared by oshtml.c and
   the banner layout in oshtml_banner.c, and nothing else */

#ifndef OSHTML_PRIV_H
#define OSHTML_PRIV_H

#include <stddef.h>

#include "os.h"
#include "glk.h"
#include "oshtml.h"
#include "osstyles.h"

/* the longest tag and entity name we keep; the rest is skipped */
#define TAG_MAX     1024
#define LINK_MAX    256
#define ENTITY_MAX  9           /* as TADS 2 has it: "thetasym" fits */
#define PENDING_MAX 256
#define OUT_MAX     256
#define TITLE_MAX   256
#define HOLD_MAX    1024        /* text after a right, center or decimal tab */
#define ELEMS_MAX   64          /* open tags we keep */
#define ELEM_NAME   12
#define TABS_MAX    64          /* <TAB ID> stops */
#define TABID_MAX   32
#define BANNER_MAX  65536       /* the text of a <BANNER> we keep */
#define MAPS_MAX    32          /* <MAP>s */
#define AREAS_MAX   48          /* <AREA>s of a map, and links in a row */
#define MAPNAME_MAX 32
#define LABEL_MAX   80
#define CELL_GAP    3           /* the spaces between table cells */
#define CELL_INDENT 2           /* after empty cells, in steps of two spaces */

enum token { TOK_TEXT, TOK_TAG, TOK_ENTITY };

/* <TAB ALIGN> */
enum tab_align { TAB_NONE, TAB_LEFT, TAB_RIGHT, TAB_CENTER, TAB_DECIMAL };

/* text attributes, as OSSTY_* bits, and colours */
typedef struct color
{
    int attr;
    os_color_t fg, bg;
    int link;                   /* osglk_link_add's number, or 0 */
} color;

/* an <AREA> of an image map, as a link */
typedef struct maplink
{
    glui32 link;
    glui32 label[LABEL_MAX];
    size_t len;
} maplink;

/* the innermost list */
enum list_kind { LIST_NONE, LIST_UL, LIST_OL };

/* an open tag, and the state from before its start tag */
typedef struct elem
{
    char name[ELEM_NAME];
    color cur;
    int indent;
    int cell_indent;
    enum list_kind list;
    int nitem;
    int nobr;
    int table;
    int ncell;
    int table_box;
    int row_align;
} elem;

struct oshtml_parser
{
    /* where the text goes */
    const oshtml_sink *sink;
    void *ctx;

    /* tokenizer */
    enum token state;
    glui32 tag[TAG_MAX];
    size_t taglen;
    glui32 tagquote;            /* the quote we're inside in a tag, or 0 */
    char entity[ENTITY_MAX + 2];
    size_t entitylen;

    /* line state */
    int col;                    /* characters printed on this line */
    glui32 lastchar;            /* the last one, quotes and ) ] } aside */
    glui32 pending[PENDING_MAX]; /* spaces not yet printed... */
    color pending_color[PENDING_MAX]; /* ...their attributes... */
    char pending_quoted[PENDING_MAX]; /* ...and which are quoted */
    size_t npending;

    /*
     *   TADS 3: whether the formatter has text in its buffer, which it
     *   keeps until a flush (and it keeps the trailing spaces even then),
     *   and whether its last flush ended a line
     */
    int buffered;
    int just_nl;

    /* tag state */
    int html;                   /* \H+ is on */
    int ignore;                 /* inside <title> or <aboutbox> */
    int title;                  /* inside <title> */
    int pre;                    /* inside <pre> */
    int quote_level;            /* <q> nesting */
    glui32 titlebuf[TITLE_MAX];
    size_t titlelen;

    /* the formatter's attributes (os_set_text_attr), as OSSTY_* bits */
    int os_attr;

    /* the attributes and colours from the tags in effect */
    color cur;

    /* the block state from the tags in effect */
    int indent;                 /* left indent, in steps of two spaces */
    int cell_indent;            /* the part of it empty cells left */
    enum list_kind list;
    int nitem;                  /* the list's items so far */
    int nobr;                   /* inside <nobr> */
    int table;                  /* the <TABLE>s we're inside */
    int ncell;                  /* the cells of the row so far */
    int row_shown;              /* the row has put something out */
    int table_box;              /* the <TABLE> has a WIDTH and BGCOLOR */
    int row_align;              /* the <TR> has an ALIGN */
    int narrow_first;           /* the last cell was a few pixels wide */

    /*
     *   A row's first cell holds back its first character (cell_hold 2)
     *   until we know whether the cell is just that one, a menu's ">" say.
     *   Then it and the gap after it are fixed-pitch, like the indent of
     *   the rows whose first cell is empty, so that the next cells line
     *   up in a proportional font.
     */
    int cell_hold;              /* 1: in a first cell, 2: holding held_c */
    glui32 held_c;
    color held_color;

    /*
     *   the main window: a picture alone so far in a table cell, held back
     *   until we know whether it goes beside the page (hold_cell_picture);
     *   cell_empty is set while nothing has come since the cell began
     */
    int cell_empty;
    int cell_pic;               /* 1: a first cell's, 2: a last cell's */
    oshtml_image cell_img;
    int cell_pic_space;         /* a space came after it */
    int side_left;              /* the row's first cell went beside it */
    int margin_pic;             /* or in the margin, until the row ends */

    /*
     *   A first cell's picture goes beside the page only if the row's last
     *   cell is a picture too, as in 1893's border down both sides; else
     *   it goes in the margin (According to Cain's vignette).  The rest of
     *   the row, from the second cell's tag, waits here as the game wrote
     *   it until its end tells which (cell_row_scan).
     */
    glui32 *rowcap;
    size_t nrowcap, rowcapmax;
    int capturing;
    int row_side;               /* 1 beside, 2 margin: the row was read */

    /*
     *   TADS 2: a <BANNER>, whose text waits for its </BANNER>, and whether
     *   we're inside the tag of the text (and inside quotes in it)
     */
    int in_banner;              /* this is the banner's own parser */
    int bancap;                 /* reading the banner's text */
    oshtml_banner ban;
    glui32 *bantext;
    size_t banlen, banmax;
    size_t bantag;              /* where the tag began */
    int bantag_open;
    glui32 banquote;

    /* the open tags */
    elem elems[ELEMS_MAX];
    int nelems;

    /*
     *   the <MAP> whose <AREA>s we're reading, and the links of the
     *   pictures shown since the last text, which go under them
     */
    struct imagemap *map;
    maplink maprow[AREAS_MAX];
    int nmaprow;

    /*
     *   the line: newlines since the last visible character, whether
     *   anything visible is on it, and if so, whether it's centred
     */
    int nl_count;
    int line_open;
    int line_center;
    int tab_centered;   /* the line is centred by a <TAB ALIGN=center> */

    /* whether the window centres each combination: 0 unknown, 1, -1 */
    signed char centers[OSSTY_COMBOS];

    /* the attributes and colours the sink has */
    color sent;

    /* the window background the sink has, from <BODY BGCOLOR> */
    os_color_t screen;

    /* the text colour from <BODY>, which outlasts clearing the window */
    os_color_t text;

    /*
     *   TADS 3: a right, center or decimal <TAB>, and the text after it;
     *   or a centred line, which the window doesn't centre (center_hold)
     */
    enum tab_align tab_align;
    int center_hold;
    int tab_start;              /* the column of the <TAB> */
    int tab_to;                 /* the column of its TO, or -1 */
    size_t tab_lead;            /* the pending spaces before it */
    glui32 tab_dp;
    color tab_color;
    glui32 hold[HOLD_MAX];
    color hold_color[HOLD_MAX];
    size_t nhold;

    /* TADS 3: <TAB ID> stops */
    struct { glui32 id[TABID_MAX]; size_t len; int col; } tabs[TABS_MAX];
    int ntabs;

    /*
     *   output: the sink, or 'dst' while oshtml_print_status is running
     */
    glui32 out[OUT_MAX];
    size_t outlen;
    glui32 *dst;
    size_t dstlen, dstmax;
};

typedef struct oshtml_parser parser;

/* an <IMG> size: pixels, or with 'pct' set, per cent of the window */
typedef struct img_size
{
    glui32 num;
    int pct;
} img_size;

/* a <MAP>: its <AREA>s, as links (oshtml.c, "Image maps") */
typedef struct imagemap
{
    glui32 name[MAPNAME_MAX];
    size_t namelen;
    int nareas;
    maplink areas[AREAS_MAX];

    /* where the areas are on the picture, unless one isn't a rectangle */
    int nrects;
    oshtml_tile rects[AREAS_MAX];
    int unplaced;
} imagemap;

/* oshtml.c: what the banner layout feeds and asks its parsers */
void feed(parser *p, glui32 c);
void flush_out(parser *p);
void put_html_text(parser *p, glui32 c);
void expand_tab(parser *p, int allow_anon);
void put_maprow(parser *p);
color cur_color(parser *p);
void parse_img_size(const glui32 *v, size_t len, img_size *out);
int find_image(parser *p, const glui32 *src, size_t srclen,
               glui32 align, img_size w, img_size h, oshtml_image *img);
glui32 href_link(const glui32 *v, size_t len, int flags);
imagemap *find_map(const glui32 *v, size_t len, int create);

/* oshtml_banner.c: a <BANNER>'s text laid out, and a page of pictures */
void end_banner(parser *p);
int build_picpage(parser *p, const glui32 *t, size_t len,
                  size_t bg, size_t bglen, oshtml_pictable *pt);

#endif /* OSHTML_PRIV_H */

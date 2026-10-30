/* oshtml_scan.h -- reading HTML TADS markup, for oshtml.c and its banner
   layout: names, attributes, and scanners of a banner's whole text.  None
   of it depends on a parser's state. */

#ifndef OSHTML_SCAN_H
#define OSHTML_SCAN_H

#include <stddef.h>

#include "glk.h"

int is_alnum(glui32 c);
int is_space(glui32 c);

/* is name[0..len) s, or one of list, whatever its case? */
int tag_is(const glui32 *name, size_t len, const char *s);
int name_in(const glui32 *name, size_t len, const char *const *list);

/* the number its leading digits make */
int to_int(const glui32 *v, size_t len);

/*
 *   A tag's attributes, one at a time.  How an unquoted value is read
 *   depends on the reader:
 *
 *   ATTR_TO_SPACE: up to a space, as HTML TADS reads it.  The tags read so
 *   in TADS 3, in banners, and where their values may be file names or
 *   commands (<IMG>, <SOUND>, <A>, <MAP>, <AREA>, TADS 2's <BANNER>).
 *
 *   ATTR_WORD: the first character, then letters and digits, as the tags
 *   read the rest in TADS 2.
 *
 *   In both a name starts with a letter, a quoted string where a name
 *   would be is skipped, and the '=' has no spaces around it.
 *
 *   ATTR_RAW: as the scanners of a whole banner read them (raw_attr): a
 *   name may start with a digit, spaces may go around the '=', and an
 *   unquoted value goes up to a space.  An attribute without a value has
 *   it where one would start, of no length.
 */
enum attr_reading { ATTR_TO_SPACE, ATTR_WORD, ATTR_RAW };

typedef struct attr_iter
{
    const glui32 *t;
    size_t i, e;                /* the rest of the tag, t[i..e) */
    enum attr_reading how;
    size_t name, namelen;       /* the attribute found: its name */
    size_t v, vlen;             /* and its value */
} attr_iter;

void attr_start(attr_iter *it, const glui32 *t, size_t s, size_t e,
                enum attr_reading how);
int attr_next(attr_iter *it);

/* the tags of a whole text, as its scanners read them */
int raw_tag(const glui32 *t, size_t len, size_t *i, size_t *s, size_t *e);
size_t raw_name(const glui32 *t, size_t s, size_t e, size_t *name, int *end);
int raw_attr(const glui32 *t, size_t s, size_t e, const char *attr,
             size_t *v, size_t *vlen);
int raw_int(const glui32 *t, size_t s, size_t e, const char *attr, int dflt);

/* what a banner's text is, before it is laid out */
int find_pictable(const glui32 *t, size_t len, size_t *s, size_t *e,
                  size_t *m);
int page_quiet_tag(const glui32 *t, size_t len);
int find_picpage(const glui32 *t, size_t len, size_t *bg, size_t *bglen);
int only_tags(const glui32 *t, size_t s, size_t e);
int cell_chars(const glui32 *t, size_t s, size_t e);
int cell_row_scan(const glui32 *s, size_t n);
int map_table_scan(const glui32 *t, size_t len);

#endif /* OSHTML_SCAN_H */

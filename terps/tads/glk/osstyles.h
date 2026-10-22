/* osstyles.h -- the 11 Glk styles, chosen per game */

#ifndef OSSTYLES_H
#define OSSTYLES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "glk.h"

/*
 *   A combination of the text attributes we keep from HTML: bold, italic
 *   (underline included), fixed pitch, the TADS-Input font, and centring
 *   (right alignment included).  Fixed and Input never come together.
 */
#define OSSTY_BOLD    0x01
#define OSSTY_ITALIC  0x02
#define OSSTY_FIXED   0x04
#define OSSTY_INPUT   0x08
#define OSSTY_CENTER  0x10
#define OSSTY_COMBOS  0x20

/*
 *   Count how often the strings in a TADS 2 or TADS 3 game file show text
 *   in each combination.  'counts' gets OSSTY_COMBOS entries.  With
 *   'escapes' zero, TADS 2's \( and \) are not counted as bold.  Returns
 *   zero if the file isn't a game file we can read.
 */
int osstyles_count(const char *filename, unsigned long *counts, int escapes);

/*
 *   Choose the styles for the game in 'filename', or the default table if
 *   it can't be read.  Call this before the first window opens.
 */
void osstyles_choose(const char *filename);

/* the same, from counts that osstyles_count made (all zero: the default) */
void osstyles_choose_counts(const unsigned long *counts);

/* set (or clear) the hints of the chosen styles for windows of this type */
void osstyles_set_hints(glui32 wintype);
void osstyles_clear_hints(glui32 wintype);

/*
 *   The style that shows a combination: its own, or the nearest one that
 *   has a style
 */
glui32 osstyles_style(int combo);

/* the combination in a style, or -1 if none (for the tests) */
int osstyles_combo(glui32 style);

#ifdef __cplusplus
}
#endif

#endif /* OSSTYLES_H */

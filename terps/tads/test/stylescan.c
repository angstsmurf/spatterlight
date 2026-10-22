/* stylescan.c -- glk/osstyles.c on its own, for check_styles.py
 *
 *   cc -I../glk -I../../../glkimp -o stylescan stylescan.c ../glk/osstyles.c
 *   stylescan [-n] GAME...
 *
 * For each game, one line: the file, a tab, then the counts as combo:count
 * (combo as osstyles.h has it, in hex), a tab, and the style chosen for
 * each combination in use as style=combo.  -n leaves out TADS 2's \( and
 * \), as stylecount.py does.
 */

#include <stdio.h>
#include <string.h>

#include "osstyles.h"

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
    (void)wintype; (void)styl; (void)hint; (void)val;
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
    (void)wintype; (void)styl; (void)hint;
}

int main(int argc, char **argv)
{
    unsigned long counts[OSSTY_COMBOS];
    int escapes = 1, i, c;
    glui32 s;

    for (i = 1 ; i < argc ; ++i)
    {
        if (strcmp(argv[i], "-n") == 0)
        {
            escapes = 0;
            continue;
        }
        if (!osstyles_count(argv[i], counts, escapes))
        {
            printf("%s\t-\t-\n", argv[i]);
            continue;
        }
        printf("%s\t", argv[i]);
        for (c = 0 ; c < OSSTY_COMBOS ; ++c)
            if (counts[c])
                printf("%x:%lu ", c, counts[c]);
        printf("\t");
        osstyles_choose_counts(counts);
        for (s = 0 ; s < 11 ; ++s)
            if (osstyles_combo(s) >= 0)
                printf("%u=%x ", s, osstyles_combo(s));
        printf("\n");
    }
    return 0;
}

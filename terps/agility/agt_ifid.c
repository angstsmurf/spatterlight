/* agt_ifid.c -- CLI wrapper around agt_copy_ifid_for_file().                */
/*                                                                           */
/* Prints the Treaty-of-Babel IFID (AGT-NNNNN-XXXXXXXX) of a raw AGT game,   */
/* computed directly from its .D$$/.DA1 data files (no AGX conversion).      */
/* Used to validate the in-process IFID path against agt2agx + babel.        */
/*                                                                           */
/* This program may be redistributed under the terms of the                 */
/* GNU General Public License, version 2; see agility.h for details.         */

#include <stdio.h>
#include <stdlib.h>
#include "agtifid.h"

int main(int argc, char *argv[])
{
    int i, rv = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <game.d$$ | game.da1> [more games...]\n",
                argv[0]);
        return 2;
    }

    /* Loop over several games in one process to also exercise re-callability. */
    for (i = 1; i < argc; i++) {
        char *ifid = agt_copy_ifid_for_file(argv[i]);
        if (ifid) {
            if (argc > 2)
                printf("%s\t%s\n", ifid, argv[i]);
            else
                printf("%s\n", ifid);
            free(ifid);
        } else {
            fprintf(stderr, "agt_ifid: could not read AGT game '%s'\n", argv[i]);
            rv = 1;
        }
    }
    return rv;
}

/* main() and the front-end globals for the headless text builds of the Plus
   and TaylorMade interpreters (plus_hl, taylor_hl): the interpreter on plain
   CheapGlk, commands on stdin, transcript on stdout, for the command script
   tests (test/scripts/README.md).

   CheapGlk's own main() is left out. It hands the game file on only if the
   interpreter lists a nameless argument in glkunix_arguments, and neither of
   these does.

   CheapGlk refuses the graphics window, so nothing is drawn. The pictures have
   tests of their own (test/images/README.md). */

#include <stdint.h>
#include <stdio.h>

#include "glk.h"
#include "glkstart.h"

extern void gli_initialize_misc(void);
extern int gli_determinism;
extern int gli_script_keys;

/* Spatterlight front-end settings (glkimp.h). No delays, and the "nothing
   forced" value for the rest. */
int gli_enable_graphics = 0;
int gli_sa_delays = 0;
int gli_sa_inventory = 0;
int gli_sa_palette = 0;
uint32_t gfgcol = 0x000000;
uint32_t gbgcol = 0xffffff;

/* What CheapGlk's main.c would define. */
int gli_screenwidth = 80;
int gli_screenheight = 24;
int gli_utf8output = 0;
int gli_utf8input = 0;
int gli_debugger = 0;

int gli_get_dataresource_info(int num, void **ptr, glui32 *len, int *isbinary)
{
    (void)num; (void)ptr; (void)len; (void)isbinary;
    return 0;
}

void win_beep_zx(int duration, int pitch)
{
    (void)duration; (void)pitch;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <game file>\n", argv[0]);
        return 2;
    }

    gli_initialize_misc();
    /* A fixed random seed, so that a transcript can be compared with the
       last one. */
    gli_determinism = 1;
    /* The scripts were written in the app: see cgmisc.c. */
    gli_script_keys = 1;

    char *game_argv[] = { argv[0], argv[1], NULL };
    glkunix_startup_t startdata = { 2, game_argv };
    if (!glkunix_startup_code(&startdata))
        return 2;
    glk_main();
    glk_exit();
    return 0;
}

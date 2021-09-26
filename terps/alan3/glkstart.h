/* glkstart.h: Unix-specific header file for GlkTerm, CheapGlk, and XGlk
        (Unix implementations of the Glk API).
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.eblong.com/zarf/glk/index.html
*/

/* This header defines an interface that must be used by program linked
    with the various Unix Glk libraries -- at least, the three I wrote.
    (I encourage anyone writing a Unix Glk library to use this interface,
    but it's not part of the Glk spec.)

    Because Glk is *almost* perfectly portable, this interface *almost*
    doesn't have to exist. In practice, it's small.
*/

#include "glkimp.h"

#ifndef GLK_START_H
#define GLK_START_H

#ifndef NULL
#define NULL 0
#endif

#define glkunix_arg_End (0)
#define glkunix_arg_ValueFollows (1)
#define glkunix_arg_NoValue (2)
#define glkunix_arg_ValueCanFollow (3)
#define glkunix_arg_NumberValue (4)


/* The list of command-line arguments; this should be defined in your code. */
extern glkunix_argumentlist_t glkunix_arguments[];

/* The external function; this should be defined in your code. */
extern int glkunix_startup_code(glkunix_startup_t *data);

/* Some helpful utility functions which the library makes available
   to your code. Obviously, this is nonportable; so you should
   only call it from glkunix_startup_code().
*/
extern strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode,
  glui32 rock);

#endif

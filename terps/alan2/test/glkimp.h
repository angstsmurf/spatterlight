/* Headless stand-in for the Spatterlight glkimp.h, so glkmedia.c compiles
   against CheapGlk's glk.h instead of dragging in the whole app protocol.
   Declares exactly the Spatterlight extensions glkmedia.c uses; the
   definitions live in headless_stubs.c. */

#ifndef GLKINT_H
#define GLKINT_H

#include "glk.h"

extern int gli_enable_graphics;
extern int gli_enable_sound;

glui32 gli_add_resource_from_path(glui32 usage, const char *path,
                                  glui32 offset, glui32 length);

#endif

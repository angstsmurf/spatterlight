#ifndef _GLKMEDIA_H_
#define _GLKMEDIA_H_

/*----------------------------------------------------------------------*\

  glkmedia.c

  Map the DOS helper commands Alan 2 games run through the SYSTEM
  instruction (VIEWER.EXE, SBPLAY.EXE, COMMAND.COM's pause) onto Glk
  images and sound channels. Spatterlight only.

\*----------------------------------------------------------------------*/

#ifdef SPATTERLIGHT
extern void glkmedia_system(char *command);
#endif

#endif

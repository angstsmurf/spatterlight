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

/* Text on its way to the main window passes through here, so that a
   description the game repeats on the far side of a picture can be held
   back and dropped. TRUE means the string was dealt with and must not be
   printed by the caller. Flush where the text stops and something else
   takes over -- before input, at the end of the game -- to release
   anything still held. */
extern int glkmedia_filter_output(char *str);
extern void glkmedia_flush_output(void);
#endif

#endif

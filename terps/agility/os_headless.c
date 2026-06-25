/* os_none.c -- interface routine template for generic system */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */
/*                                                        */
/* This is part of the source for AGiliTy, the (Mostly) Universal  */
/*       AGT Interpreter                                           */

/* This should work on any system supporting ANSI C, at the price
     of playing without a status line or other system-specific 
     i/o features. */



#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <ctype.h>
#include <limits.h>

#include "agility.h"
#include "interp.h"


int scroll_count; /* Count of how many lines we have printed since
		     last player input. */
void print_statline(void);




void agt_delay(int n)
/* This should wait for n seconds */
{
  if (fast_replay || BATCH_MODE) return;
  print_statline();
  /* sleep(n);*/
}


void agt_tone(int hz,int ms)
/* Produce a hz-Hertz sound for ms milliseconds */
{
  return;
}

int agt_rand(int a,int b)
/* Return random number from a to b inclusive */
{
    return a+(rand()>>2)%(b-a+1);
}


/* ------------------------------------------------------------------------
   Faithful replica of Spatterlight's CommandScriptHandler (the Cocoa class
   that feeds a saved command script to the game during UITests).  Getting this
   exactly right matters: the script is RNG/turn-count sensitive and mixes line
   input (commands) with char input (yes/no, "press any key").

   The Spatterlight model:
   - LINE request -> next script line is sent as the command.
   - CHAR request -> chars are taken one at a time from an "untyped" buffer;
     when the buffer is empty the next script line is loaded into it.  A line of
     length 1 is buffered as the single char (no trailing newline); a longer
     line is buffered as line+"\n".
   - If a LINE request arrives while the untyped buffer still holds >1 char
     (i.e. a char request only nibbled the front of a line), Spatterlight backs
     up and re-emits that whole line as the command.  This is the trick that
     lets e.g. the post-"howl" "[Press any key]" take the 't' of "take nickel"
     while "take nickel" is still delivered intact as the next command.
   - yes/no answers in the script are written " y" / " n" (leading space) so the
     first getchar of yesno() sees the space (-> reprompt) and the second sees
     the real answer.

   "Press any key" waits never reach agt_getkey (agt_waitkey returns early under
   fast_replay, set in init_interface), matching how the replay skips them.

   Exception: the pre-game startup "Choose <I>nstructions..." menu calls
   agt_getchar before the first real command; that menu is not part of the
   command script in Spatterlight, so we auto-dismiss it without consuming a
   line. */

int hl_commands_started = 0;

static char cs_untyped[8192];     /* mirrors _untypedCharacters */
static int  cs_untyped_len = 0;
static char cs_curline[8192];     /* full original line feeding cs_untyped */
static int  cs_have_curline = 0;

/* Read the next raw script line from stdin (newline trimmed); skip "# " comment
   lines.  Returns 0 on EOF. */
static int cs_read_raw_line(char *out, int outsz)
{
  for (;;) {
    int n;
    if (fgets(out, outsz, stdin) == NULL) return 0;
    n = (int)strlen(out);
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = 0;
    if (out[0] == '#' && out[1] == ' ') continue;
    return 1;
  }
}

char *agt_input(int in_type)
/* LINE request.  in_type: 0=command,1=number,2=question,3=userstr,4=filename,
   5=RESTART/RESTORE/UNDO/QUIT.  Negative is internal (unused here). */
{
  static int input_error = 0;
  char *s = rmalloc(8192);

  hl_commands_started = 1;
  scroll_count = 0;
  print_statline();

  if (cs_untyped_len > 1 && cs_have_curline) {
    /* Back up: a char request nibbled this line; re-emit it as the command. */
    strcpy(s, cs_curline);
    input_error = 0;
  } else {
    char raw[8192];
    if (!cs_read_raw_line(raw, sizeof raw)) {
      if (++input_error > 25) { printf("Read error on input\n"); exit(EXIT_FAILURE); }
      s[0] = 0;
    } else {
      strcpy(s, raw);
      input_error = 0;
    }
  }
  cs_untyped_len = 0; cs_untyped[0] = 0; cs_have_curline = 0;

  /* Echo the command (so the captured transcript shows what was typed). */
  if (in_type >= 0) printf("%s\n", s);
  if (DEBUG_OUT) fprintf(debugfile, "%s\n", s);
  if (script_on) textputs(scriptfile, s);
  return s;
}

char agt_getkey(rbool echo_char)
/* CHAR request. */
{
  char key;

  if (!hl_commands_started)
    return ' ';                 /* auto-dismiss the startup <I/A/other> menu */

  if (cs_untyped_len == 0) {
    char raw[8192];
    int len;
    if (!cs_read_raw_line(raw, sizeof raw))
      return ' ';               /* EOF: harmless filler */
    strcpy(cs_curline, raw);
    cs_have_curline = 1;
    len = (int)strlen(raw);
    if (len == 1) {
      cs_untyped[0] = raw[0]; cs_untyped[1] = 0; cs_untyped_len = 1;
    } else {
      strcpy(cs_untyped, raw);
      cs_untyped[len] = '\n'; cs_untyped[len+1] = 0; cs_untyped_len = len + 1;
    }
  }

  key = cs_untyped_len > 0 ? cs_untyped[0] : '\n';
  if (key == '\r') key = '\n';

  if (cs_untyped_len > 1) {
    memmove(cs_untyped, cs_untyped + 1, cs_untyped_len); /* includes the NUL */
    cs_untyped_len--;
  } else {
    cs_untyped_len = 0; cs_untyped[0] = 0; cs_have_curline = 0;
  }
  return key;
}

void agt_textcolor(int c)
/* Set text color to color #c, where the colors are as follows: */
/*  0=Black, 1=Blue,    2=Green, 3=Cyan, */
/*  4=Red,   5=Magenta, 6=Brown,  */
/*  7=Normal("White"-- which may actually be some other color) */
/*     This should turn off blinking, bold, color, etc. and restore */
/*     the text mode to its default appearance. */
/*  8=Turn on blinking.  */
/*  9= *Just* White (not neccessarily "normal" and no need to turn off */
/*      blinking)  */
/* 10=Turn on fixed pitch font.  */
/* 11=Turn off fixed pitch font */
/* Also used to set other text attributes: */
/*   -1=emphasized text, used (e.g.) for room titles */
/*   -2=end emphasized text */
{
#if 0
  if (c==-1) writestr("<<");
  if (c==-2) writestr(">>");
#endif
  return;
}



void agt_statline(const char *s)
/* Output a string on the status line, if possible */
{
  return;
}


void agt_clrscr(void)
/* This should clear the screen and put the cursor at the upper left
  corner (or one down if there is a status line) */
{
  /* Headless: don't emit a screenful of blank lines, just reset state. */
  if (DEBUG_OUT) fprintf(debugfile,"\n\n<CLRSCR>\n\n");
  if (script_on) textputs(scriptfile,"\n\n\n\n");
  scroll_count=0;
}


/* Runaway guard: if the script runs out before the game ends (e.g. the player
   dies and we hit the restart/restore/undo/quit prompt with EOF on stdin), the
   engine can re-print prompts indefinitely.  Cap total output so a stuck run
   can never fill the disk; exit cleanly once the cap is hit. */
#define HEADLESS_MAX_OUTPUT (16L*1024L*1024L)  /* 16 MB is plenty for a full win */
static long headless_output_bytes = 0;

static void headless_count_output(long n)
{
  headless_output_bytes += n;
  if (headless_output_bytes > HEADLESS_MAX_OUTPUT) {
    fflush(stdout);
    fprintf(stderr, "\n[headless: output cap (%ldMB) reached -- aborting run]\n",
            HEADLESS_MAX_OUTPUT/(1024L*1024L));
    exit(EXIT_FAILURE);
  }
}

void agt_puts(const char *s)
{
  printf("%s",s);
  curr_x+=strlen(s);
  headless_count_output((long)strlen(s));
  if (DEBUG_OUT) fprintf(debugfile,"%s",s);
  if (script_on) textputs(scriptfile,s);
}

void agt_newline(void)
{
  printf("\n");
  curr_x=0;
  headless_count_output(1);
  if (DEBUG_OUT) fprintf(debugfile,"\n");
  if (script_on) textputs(scriptfile,"\n");
  scroll_count++;
  if (0 && scroll_count>=screen_height-2) {
    printf("  --MORE--");  /* If possible, this should disappear after 
			      a key is pressed */
    agt_waitkey();
  }
}


static unsigned long boxflags;
static int box_startx; /* Starting x of box */
static int box_width;
static int delta_scroll; /* Amount we are adding to vertical scroll 
			    with the box */
static void boxrule(void)
/* Draw line at top or bottom of box */
{ 
  int i;
  
  agt_puts("+");
  for(i=0;i<box_width;i++) agt_puts("-");
  agt_puts("+");
}

static void boxpos(void)
{
  int i;

  agt_newline();
  for(i=0;i<box_startx;i++) agt_puts(" ");
}

void agt_makebox(int width,int height,unsigned long flags)
/* Flags: TB_TTL, TB_BORDER */
{
  boxflags=flags;
  box_width=width;
  if (boxflags&TB_BORDER) width+=2;  /* Add space for border */
  box_startx=(screen_width-width)/2; /* Center the box horizontally */
  delta_scroll=height;
  boxpos();
  if (boxflags&TB_BORDER) {
    boxrule();
    boxpos();
    agt_puts("|");
  }
}

void agt_qnewline(void)
{
  if (boxflags&TB_BORDER) agt_puts("|");
  boxpos();
  if (boxflags&TB_BORDER) agt_puts("|");
}

void agt_endbox(void)
{
  if (boxflags&TB_BORDER) {
    agt_puts("|");
    boxpos();
    boxrule();
  }
  scroll_count+=delta_scroll;
  agt_newline(); /* NOT agt_qnewline() */
}


#define opt(s) (!strcmp(optstr[0],s))

rbool agt_option(int optnum,char *optstr[],rbool setflag)
/* If setflag is 0, then the option was prefixed with NO_ */
{
  /* Return 1 if the option is recognized */
  return 0; 
}

#undef opt

genfile agt_globalfile(int fid)
{
  if (fid==0) 
    return badfile(fCFG); /* Should return FILE* to open
			      global configuration file */
  else return badfile(fCFG);
}


void init_interface(int argc,char *argv[])
{
  script_on=0;scriptfile=badfile(fSCR);
  scroll_count=0;
  center_on=par_fill_on=0;
  debugfile=stderr;
  DEBUG_OUT=0; /* If set, we echo all output to debugfile */
       /* This should be 1 if stderr points to a different */
       /* device than stdin. (E.g. to a file instead of to the */
       /* console) */
  screen_height=1000000;  /* Headless: never page (no --MORE-- prompts) */
  status_width=screen_width=80;
  /* Replay mode: skip "press any key" waits and delays (agt_waitkey/agt_delay
     return early), exactly as Spatterlight does while replaying a command log.
     Char input that the game actually reads (yes/no via agt_getchar) still
     consumes a script line in agt_getkey. */
  fast_replay=1;
  agt_clrscr();
}

void start_interface(fc_type fc)
{
  if (stable_random)  
    srand(6);
  else 
    srand(time(0));
}

void close_interface(void)
{
  if (filevalid(scriptfile,fSCR))
    close_pfile(scriptfile,0);
}

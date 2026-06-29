/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 3 harness (turn loop).
 *
 * Loads an ADRIFT 5 game, prints the introduction + opening LOOK, then feeds a
 * command script (one command per line, from argv[2] or stdin) through the turn
 * loop, echoing each command and the resulting output -- so movement, take/drop,
 * open and examine can be eyeballed against frankendrift / the official Runner.
 *
 *   ./a5run_dump <game.blorb> [script.txt]
 *
 * A5_TRACE_RUN=1   trace task matching + action execution
 * A5_TRACE_RESTR=1 trace restriction evaluation
 * A5_SAVE_AT=N     after the Nth command, a5run_save the state, free the run,
 *                  build a fresh run, a5run_restore it, and continue from there
 *                  -- a save/restore self-check: the transcript must be identical
 *                  to a plain run (Phase 5).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5model.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5state.h"

int
main (int argc, char **argv)
{
  a5_adventure_t *a;
  a5_run_t *run;
  char *txt;
  FILE *script = stdin;
  char line[1024];

  if (argc < 2)
    {
      fprintf (stderr, "usage: %s <game.blorb|game.taf> [script.txt]\n", argv[0]);
      return 2;
    }
  if (getenv ("A5_TRACE_RUN") != NULL)   a5run_trace = 1;
  if (getenv ("A5_TRACE_RESTR") != NULL) a5restr_trace = 1;

  a = a5model_load (argv[1]);
  if (a == NULL)
    {
      fprintf (stderr, "a5run_dump: failed to load %s\n", argv[1]);
      return 1;
    }
  run = a5run_new (a);
  if (run == NULL)
    {
      fprintf (stderr, "a5run_dump: out of memory\n");
      a5model_free (a);
      return 1;
    }

  if (argc >= 3)
    {
      script = fopen (argv[2], "r");
      if (script == NULL)
        {
          fprintf (stderr, "a5run_dump: cannot open %s\n", argv[2]);
          a5run_free (run);
          a5model_free (a);
          return 1;
        }
    }

  /* FrankenDrift opens with the centered title ("<c>Title</c>" + newline) then
     the introduction directly -- no separating blank (the intro supplies its own
     leading blank when it has one).  Match that so the ground-truth diff is free
     of harness noise. */
  printf ("%s\n", a->title ? a->title : "(none)");
  txt = a5run_intro (run);
  printf ("%s\n", txt);
  free (txt);

  {
    const char *sa = getenv ("A5_SAVE_AT");
    long save_at = sa != NULL ? strtol (sa, NULL, 10) : -1;
    long cmd_no = 0;

    while (fgets (line, sizeof line, script) != NULL)
      {
        size_t n = strlen (line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
          line[--n] = '\0';
        if (n == 0 || line[0] == '#')
          continue;
        printf ("\n> %s\n", line);
        txt = a5run_input (run, line);
        printf ("%s\n", txt);
        free (txt);
        if (a5run_is_over (run))
          /* The engine has already emitted the win/lose/score/restart block
             (clsUserSession.CheckEndOfGame); nothing more to print. */
          break;
        if (++cmd_no == save_at)
          {
            /* Save, tear the run down, rebuild + restore, and continue: the rest
               of the transcript must match a plain run. */
            size_t len = 0;
            char *blob = a5run_save (run, &len);
            a5run_free (run);
            run = a5run_new (a);
            if (blob == NULL || !a5run_restore (run, blob, len))
              { fprintf (stderr, "a5run_dump: save/restore failed\n"); free (blob); break; }
            free (blob);
            fprintf (stderr, "[restored after command %ld]\n", cmd_no);
          }
      }
  }

  if (script != stdin)
    fclose (script);
  a5run_free (run);
  a5model_free (a);
  return 0;
}

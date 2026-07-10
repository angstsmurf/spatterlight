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
 * A5_UNDO_AT=N     at the Nth command, a5run_snapshot, run it (captured), then
 *                  a5run_undo and re-run the SAME command -- an undo self-check:
 *                  the two runs must be byte-identical (undo fully reverts state
 *                  AND the RNG) and a second consecutive undo must fail; the
 *                  printed transcript stays identical to a plain run.
 * A5_CKPT_SAVE=F   after the whole (prefix) script has run, a5run_save the state
 *                  to file F.  A5_CKPT_RESTORE=F restores that blob instead of
 *                  starting fresh (the intro/opening LOOK is skipped) and then
 *                  feeds the given (tail) script from the checkpoint.  Since the
 *                  save preserves the RNG byte-for-byte (see A5_SAVE_AT), this
 *                  turns an O(n^2) "replay the whole growing script every align
 *                  attempt" search into O(prefix) once + O(tail) per attempt --
 *                  the speedup needed to align the weather-gated Tingalan windows.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5model.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5state.h"
#include "a5text.h"

/* One-line pushback over the command script: the Adventure-Upgrade question
   below peeks at the next meaningful line and puts it back when it is not a
   literal yes/no answer.  Every script reader (the main loop, %PopUpInput%,
   the upgrade question) goes through script_gets so they never fight over a
   line. */
static char script_pushback[1024];
static int have_pushback = 0;

static char *
script_gets (char *buf, size_t sz, FILE *f)
{
  if (have_pushback)
    {
      snprintf (buf, sz, "%s", script_pushback);
      have_pushback = 0;
      return buf;
    }
  return fgets (buf, (int) sz, f);
}

/* %PopUpInput% side channel: feed the next meaningful line of the command
   script as the answer to a naming prompt (skipping blank/# lines exactly like
   the main command loop, so the two never fight over a line).  Returns a
   heap-allocated answer the engine takes ownership of, or NULL at EOF (the
   engine then falls back to the prompt's default).  FrankenDrift.Headless
   consumes one script line per popup the same way, keeping transcripts aligned. */
static char *
popup_from_script (void *ctx, const char *prompt, const char *dflt)
{
  (void) prompt; (void) dflt;
  FILE *f = (FILE *) ctx;
  char buf[1024];
  while (script_gets (buf, sizeof buf, f) != NULL)
    {
      size_t n = strlen (buf);
      while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = '\0';
      if (n == 0 || buf[0] == '#')
        continue;
      return strdup (buf);
    }
  return NULL;
}

/* The "Adventure Upgrade" bracket-correction question, answered through the
   ordinary command script exactly like FrankenDrift.Headless AskYesNoQuestion:
   print the question, then consume the next meaningful script line only when
   it is literally yes/y/no/n (trimmed, case-insensitive); anything else is
   pushed back for the main loop and the answer defaults to no.  On yes, echo
   FD's Glue.ShowInfo "N tasks have been updated." -- both writes are raw
   (no trailing break), so the game title abuts them just as in FD. */
static void
ask_upgrade_question (a5_adventure_t *a, FILE *script)
{
  char buf[1024];
  int yes = 0, n;
  if (!a5model_upgrade_pending (a))
    return;
  fputs ("Adventure Upgrade\n", stdout);
  fputs (a5model_upgrade_question (), stdout);
  while (script_gets (buf, sizeof buf, script) != NULL)
    {
      size_t len = strlen (buf);
      char *p = buf;
      while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
      if (len == 0 || buf[0] == '#')
        continue;
      while (*p == ' ' || *p == '\t')
        p++;
      {
        char *e = p + strlen (p);
        while (e > p && (e[-1] == ' ' || e[-1] == '\t'))
          *--e = '\0';
      }
      if (strcasecmp (p, "yes") == 0 || strcasecmp (p, "y") == 0)
        yes = 1;
      else if (strcasecmp (p, "no") == 0 || strcasecmp (p, "n") == 0)
        yes = 0;
      else
        {
          /* Not an answer: push the line back and answer no (FD peeks). */
          snprintf (script_pushback, sizeof script_pushback, "%s", buf);
          have_pushback = 1;
        }
      break;
    }
  n = a5model_upgrade_answer (a, yes);
  if (yes)
    printf ("Adventure Upgrade\n%d tasks have been updated.", n);
}

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

  if (argc >= 3)
    {
      script = fopen (argv[2], "r");
      if (script == NULL)
        {
          fprintf (stderr, "a5run_dump: cannot open %s\n", argv[2]);
          a5model_free (a);
          return 1;
        }
    }

  /* The Adventure-Upgrade bracket-correction question is answered from the
     command script (a literal leading yes/no line), before the run is built,
     mirroring FD's ask-at-load. */
  ask_upgrade_question (a, script);

  run = a5run_new (a);
  if (run == NULL)
    {
      fprintf (stderr, "a5run_dump: out of memory\n");
      a5model_free (a);
      return 1;
    }

  /* Let %PopUpInput% prompts (naming puzzles) pull their answer from the next
     script line, so they are scriptable and reproducible. */
  a5text_set_popup_cb (popup_from_script, script);

  /* Checkpoint restore: load a saved blob (from a prior A5_CKPT_SAVE run) and
     continue from there, skipping the intro.  Mirrors the A5_SAVE_AT self-check's
     free/new/restore, but the blob comes from a file, so a fixed prefix can be
     replayed once and many alignment tails fed cheaply. */
  const char *ckpt_restore = getenv ("A5_CKPT_RESTORE");
  if (ckpt_restore != NULL)
    {
      FILE *cf = fopen (ckpt_restore, "rb");
      if (cf == NULL)
        { fprintf (stderr, "a5run_dump: cannot open checkpoint %s\n", ckpt_restore);
          a5run_free (run); a5model_free (a); return 1; }
      fseek (cf, 0, SEEK_END);
      long clen = ftell (cf);
      fseek (cf, 0, SEEK_SET);
      char *blob = (char *) malloc (clen > 0 ? (size_t) clen : 1);
      size_t got = clen > 0 ? fread (blob, 1, (size_t) clen, cf) : 0;
      fclose (cf);
      if (!a5run_restore (run, blob, got))
        { fprintf (stderr, "a5run_dump: checkpoint restore failed\n");
          free (blob); a5run_free (run); a5model_free (a); return 1; }
      free (blob);
      a5run_undo_forget (run);
      fprintf (stderr, "[checkpoint restored from %s]\n", ckpt_restore);
    }
  else
    {
      /* a5run_intro now emits the centred title itself (mirroring FD's
         Display("<c>" & Adventure.Title & "</c>") at clsUserSession init), so that
         any System <RunImmediately> title-music task's output can join onto it via
         pSpace -- the harness no longer prints the title separately. */
      txt = a5run_intro (run);
      printf ("%s\n", txt);
      free (txt);
    }

  {
    const char *sa = getenv ("A5_SAVE_AT");
    long save_at = sa != NULL ? strtol (sa, NULL, 10) : -1;
    const char *ua = getenv ("A5_UNDO_AT");
    long undo_at = ua != NULL ? strtol (ua, NULL, 10) : -1;
    long cmd_no = 0;

    while (script_gets (line, sizeof line, script) != NULL)
      {
        size_t n = strlen (line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
          line[--n] = '\0';
        if (n == 0 || line[0] == '#')
          continue;
        printf ("\n> %s\n", line);
        if (cmd_no + 1 == undo_at)
          {
            /* Undo self-check: snapshot, run the command, undo it, re-run the
               SAME command; the two outputs must match byte-for-byte (undo also
               reverts the RNG), and a second undo must now fail. */
            char *first;
            a5run_snapshot (run);
            first = a5run_input (run, line);
            if (!a5run_undo (run))
              { fprintf (stderr, "a5run_dump: undo returned no snapshot\n"); free (first); break; }
            if (a5run_undo (run))
              { fprintf (stderr, "a5run_dump: second consecutive undo unexpectedly succeeded\n"); free (first); break; }
            txt = a5run_input (run, line);
            if (strcmp (first, txt) != 0)
              { fprintf (stderr, "a5run_dump: undo re-run diverged from first run\n"); free (first); free (txt); break; }
            free (first);
            fprintf (stderr, "[undo self-check ok at command %ld]\n", undo_at);
            printf ("%s\n", txt);
            free (txt);
            if (a5run_is_over (run))
              break;
            ++cmd_no;
            continue;
          }
        txt = a5run_input (run, line);
        printf ("%s\n", txt);
        free (txt);
        /* A5_DUMP_VARS="Depth,Encountern,..." prints those vars + the player's
           location to stderr after each command -- navigation aid for deriving a
           deterministic walkthrough of a large map/encounter game (Tingalan). */
        {
          static const char *dv = (const char *) 1;
          if (dv == (const char *) 1) dv = getenv ("A5_DUMP_VARS");
          if (dv != NULL)
            {
              a5_state_t *st = a5run_state (run);
              const char *pk = a5state_player_key (st);
              int ci = pk ? a5state_character_index (st, pk) : -1;
              fprintf (stderr, "[loc=%s", (ci >= 0 && st->char_loc[ci]) ? st->char_loc[ci] : "?");
              const char *p = dv;
              char nm[64];
              while (*p)
                {
                  size_t k = 0;
                  while (*p && *p != ',' && k < sizeof nm - 1) nm[k++] = *p++;
                  nm[k] = '\0';
                  if (*p == ',') p++;
                  long val = 0;
                  if (k && a5state_var_num_by_name (st, nm, &val))
                    fprintf (stderr, " %s=%ld", nm, val);
                }
              fprintf (stderr, "]\n");
            }
        }
        /* A5_DUMP_CHARS=1 prints every character's current location to stderr
           after each command (walkthrough-derivation aid for tracking
           patrolling NPCs in Six Silver Bullets). */
        {
          static const char *dc = (const char *) 1;
          if (dc == (const char *) 1) dc = getenv ("A5_DUMP_CHARS");
          if (dc != NULL)
            {
              a5_state_t *st = a5run_state (run);
              const a5_adventure_t *a2 = st->adv;
              fprintf (stderr, "[chars");
              for (int i = 0; i < a2->n_characters; i++)
                {
                  const char *ck = a2->characters[i].key;
                  int ci = a5state_character_index (st, ck);
                  const char *cl = (ci >= 0 && st->char_loc[ci]) ? st->char_loc[ci] : "?";
                  fprintf (stderr, " %s@%s", ck, cl);
                }
              fprintf (stderr, "]\n");
            }
        }
        /* A5_DUMP_OBJS=1 prints every non-hidden object's location to stderr
           after each command; A5_DUMP_OBJS=key1,key2 restricts to those object
           keys (walkthrough-derivation aid for randomly-placed objects, e.g.
           The Salvage's derelicts). */
        {
          static const char *dobj = (const char *) 1;
          if (dobj == (const char *) 1) dobj = getenv ("A5_DUMP_OBJS");
          if (dobj != NULL)
            {
              a5_state_t *st = a5run_state (run);
              const a5_adventure_t *a2 = st->adv;
              int filter = strcmp (dobj, "1") != 0;
              fprintf (stderr, "[objs");
              for (int i = 0; i < a2->n_objects; i++)
                {
                  const char *ok = a2->objects[i].key;
                  if (filter)
                    {
                      const char *hit = strstr (dobj, ok);
                      if (hit == NULL)
                        continue;
                    }
                  else if (st->obj[i].where == A5_OWHERE_HIDDEN
                           || st->obj[i].where == A5_OWHERE_NONE)
                    continue;
                  fprintf (stderr, " %s@%d:%s", ok, (int) st->obj[i].where,
                           st->obj[i].key != NULL ? st->obj[i].key : "-");
                }
              fprintf (stderr, "]\n");
            }
        }
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

  /* A5_CKPT_SAVE: after the whole (prefix) script has run, serialise the state
     to a file so a later A5_CKPT_RESTORE run can continue from here without
     replaying the prefix. */
  {
    const char *ckpt_save = getenv ("A5_CKPT_SAVE");
    if (ckpt_save != NULL && !a5run_is_over (run))
      {
        size_t len = 0;
        char *blob = a5run_save (run, &len);
        FILE *cf = blob != NULL ? fopen (ckpt_save, "wb") : NULL;
        if (cf == NULL || fwrite (blob, 1, len, cf) != len)
          fprintf (stderr, "a5run_dump: checkpoint save to %s failed\n", ckpt_save);
        else
          fprintf (stderr, "[checkpoint saved to %s (%zu bytes)]\n", ckpt_save, len);
        if (cf != NULL) fclose (cf);
        free (blob);
      }
  }

  /* A5_DUMP_DONE: after the script ends, list every task whose "scored" flag
     is set (it has contributed to Score) plus every completed task, one per
     line on stderr -- lets a walkthrough author diff fired scoring tasks
     against the game's full scoring checklist. */
  if (getenv ("A5_DUMP_DONE") != NULL)
    {
      a5_state_t *st = a5run_state (run);
      for (int i = 0; i < st->adv->n_tasks; i++)
        if (st->task_scored[i] || st->task_done[i])
          fprintf (stderr, "[task %s%s%s]\n", st->adv->tasks[i].key,
                   st->task_done[i] ? " done" : "",
                   st->task_scored[i] ? " scored" : "");
    }

  if (script != stdin)
    fclose (script);
  a5run_free (run);
  a5model_free (a);
  return 0;
}

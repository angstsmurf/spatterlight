/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Regression test for the runner's quit/restart/restore/undo control flow.
 *
 * run_quit / run_restart / run_restore / run_undo used to longjmp(game->quitter)
 * out of a *running* game's main loop back to run_interpret.  That longjmp
 * skipped the destructors of any non-trivial C++ local live in the
 * command/task/print/expr call tree (undefined once that tree holds
 * std::string/std::vector), which blocked RAII across the runner.  They now
 * throw a file-local run_loop_halt exception that run_interpret catches, so the
 * same frames unwind with their destructors run.
 *
 * No headless harness used to drive this path -- a player typing "quit" takes
 * the flag path (lib_cmd_quit sets is_running=FALSE and returns), and only an
 * EOF / front-end quit while the game is still running reaches the re-entrant
 * scr_quit_game -> run_quit throw.  This fixture reproduces that re-entrant case
 * deterministically with no external game data, by calling the public API from
 * inside os_read_line (i.e. from within a live turn):
 *
 *   1st prompt: scr_restart_game()  -> throw, do_restart=TRUE  -> caught,
 *               run_interpret re-enters the loop (the "halt then continue"
 *               branch);
 *   2nd prompt: scr_quit_game()     -> throw, is_running=FALSE -> caught,
 *               run_interpret leaves the loop (the "halt then stop" branch).
 *
 * It then asserts scr_interpret_game returned (the throw was caught, not
 * terminated or leaked past the boundary), both branches were taken, and the
 * loop stopped (os_read_line was not called a third time).  Reuses the
 * committed precedence_test.taf.  Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string.h>

#include "scarier.h"

static int failures = 0;
static scr_game g_game = NULL;

static int g_calls = 0;        /* os_read_line invocations */
static int g_saw_restart = 0;
static int g_saw_quit = 0;
static int g_returned = 0;     /* set after scr_interpret_game returns */

static void
expect (const char *what, int got, int want)
{
  const int ok = (!got == !want);
  printf ("  [%s] %-52s got %d, want %d\n",
          ok ? "PASS" : "FAIL", what, !!got, !!want);
  if (!ok)
    failures++;
}

/* --- minimal SCARIER OS port ------------------------------------------------ */

void os_print_string (const scr_char *string) { (void) string; }
void os_print_tag (scr_int tag, const scr_char *arg) { (void) tag; (void) arg; }
void os_play_sound (const scr_char *p, scr_int o, scr_int l, scr_bool v)
{ (void) p; (void) o; (void) l; (void) v; }
void os_stop_sound (void) { }
void os_show_graphic (const scr_char *p, scr_int o, scr_int l)
{ (void) p; (void) o; (void) l; }
void os_display_hints (scr_game game) { (void) game; }
scr_bool os_confirm (scr_int type) { (void) type; return 1; }   /* yes to quit */
void *os_open_file (scr_bool is_save) { (void) is_save; return NULL; }
void os_write_file (void *o, const scr_byte *b, scr_int l)
{ (void) o; (void) b; (void) l; }
scr_int os_read_file (void *o, scr_byte *b, scr_int l)
{ (void) o; (void) b; (void) l; return 0; }
void os_close_file (void *o) { (void) o; }
void os_print_string_debug (const scr_char *s) { (void) s; }
scr_bool os_read_line_debug (scr_char *b, scr_int l) { return os_read_line (b, l); }

scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  /*
   * We are inside a live turn (run_main_loop is on the stack, is_running is
   * TRUE), exactly the re-entrant state the front end is in when it asks the
   * engine to quit/restart.  Each of these public calls throws run_loop_halt
   * and never returns to us.
   */
  switch (g_calls++)
    {
    case 0:
      g_saw_restart = 1;
      scr_restart_game (g_game);   /* throw -> caught -> loop re-enters */
      break;
    case 1:
      g_saw_quit = 1;
      scr_quit_game (g_game);      /* throw -> caught -> loop stops */
      break;
    default:
      /* Should be unreachable: the loop must have stopped after the quit. */
      break;
    }

  /* If a call ever returns here, the throw/catch wiring is broken. */
  buffer[0] = '\0';
  return 1;
}

/* -------------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "precedence_test.taf";

  g_game = scr_game_from_filename (path);
  if (!g_game)
    {
      fprintf (stderr, "quit_test: cannot load %s\n", path);
      return 1;
    }

  printf ("loaded %s\n", path);
  scr_interpret_game (g_game);
  g_returned = 1;                  /* reached iff the catch unwound cleanly */
  scr_free_game (g_game);

  expect ("scr_interpret_game returned after a mid-turn quit", g_returned, 1);
  expect ("restart branch (throw -> catch -> re-loop) taken", g_saw_restart, 1);
  expect ("quit branch (throw -> catch -> stop) taken", g_saw_quit, 1);
  expect ("loop stopped after quit (os_read_line not called again)",
          g_calls == 2, 1);

  printf ("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

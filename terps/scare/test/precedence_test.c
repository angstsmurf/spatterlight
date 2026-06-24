/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Regression test for sc_does_command_match(), the probe that lets the Glk
 * front end give author-defined commands precedence over its own single-letter
 * abbreviation expansions (e.g. "p" -> "open", "k" -> "attack", "c" -> "close").
 *
 * Many ADRIFT games use single letters as menu choices (battle/conversation
 * menus -- e.g. the attack choices in hyper_b_s.taf, or "C" in The PK Girl).
 * With abbreviation expansion on, the Glk port would silently rewrite those
 * letters; the probe lets it pass a letter through unchanged whenever the game
 * already recognises it.
 *
 * The synthetic game (make_precedence_taf.py) is two rooms with two single-
 * letter tasks scoped one-per-room: "p" works only in the West room, "k" only
 * in the East room.  Standing in each room -- exactly the state the port is in
 * when it would expand an abbreviation -- we check that sc_does_command_match()
 * claims the room's own letter and nothing else.
 *
 * Pass the .taf path as argv[1].  Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string.h>

#include "scare.h"

static int failures = 0;
static sc_game g_game = NULL;

static void
expect (const char *what, int got, int want)
{
  const int ok = (!got == !want);
  printf ("  [%s] %-54s got %d, want %d\n",
          ok ? "PASS" : "FAIL", what, !!got, !!want);
  if (!ok)
    failures++;
}

/*
 * Scripted player input.  Each line is fed in order; the optional probe runs
 * just before the line is delivered -- i.e. while the game sits at the prompt
 * this line is about to answer.
 */
struct step { const char *line; void (*probe) (void); };

static void
probe_west_room (void)
{
  /* "p" is this room's task and must win over the "p" -> "open" expansion;
   * "k" belongs to the other room, and "c"/"x" to no task at all. */
  expect ("west room: game claims 'p' (don't expand to open)",
          sc_does_command_match (g_game, "p"), 1);
  expect ("west room: game does NOT claim 'k'",
          sc_does_command_match (g_game, "k"), 0);
  expect ("west room: game does NOT claim 'c'",
          sc_does_command_match (g_game, "c"), 0);
  expect ("west room: game does NOT claim 'x'",
          sc_does_command_match (g_game, "x"), 0);
}

static void
probe_east_room (void)
{
  /* The scoping flips after moving north: "k" is live here, "p" is not. */
  expect ("east room: game claims 'k' (don't expand to attack)",
          sc_does_command_match (g_game, "k"), 1);
  expect ("east room: game does NOT claim 'p'",
          sc_does_command_match (g_game, "p"), 0);
}

static struct step SCRIPT[] = {
  { "north", probe_west_room },   /* probe at the start room, then go north */
  { "quit",  probe_east_room },   /* probe in the east room, then quit */
  { NULL,    NULL }
};

static int g_index = 0;

/* --- minimal SCARE OS port ------------------------------------------------ */

void os_print_string (const sc_char *string) { (void) string; }
void os_print_tag (sc_int tag, const sc_char *arg) { (void) tag; (void) arg; }
void os_play_sound (const sc_char *p, sc_int o, sc_int l, sc_bool v)
{ (void) p; (void) o; (void) l; (void) v; }
void os_stop_sound (void) { }
void os_show_graphic (const sc_char *p, sc_int o, sc_int l)
{ (void) p; (void) o; (void) l; }
void os_display_hints (sc_game game) { (void) game; }
sc_bool os_confirm (sc_int type) { (void) type; return 1; }   /* yes to quit */
void *os_open_file (sc_bool is_save) { (void) is_save; return NULL; }
void os_write_file (void *o, const sc_byte *b, sc_int l)
{ (void) o; (void) b; (void) l; }
sc_int os_read_file (void *o, sc_byte *b, sc_int l)
{ (void) o; (void) b; (void) l; return 0; }
void os_close_file (void *o) { (void) o; }
void os_print_string_debug (const sc_char *s) { (void) s; }
sc_bool os_read_line_debug (sc_char *b, sc_int l) { return os_read_line (b, l); }

sc_bool
os_read_line (sc_char *buffer, sc_int length)
{
  const struct step *step = &SCRIPT[g_index];

  if (!step->line)
    {
      /* Script exhausted: feed an empty line so the loop can wind down. */
      buffer[0] = '\0';
      return 1;
    }

  if (step->probe)
    step->probe ();

  strncpy ((char *) buffer, step->line, length - 1);
  buffer[length - 1] = '\0';
  g_index++;
  return 1;
}

/* -------------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "precedence_test.taf";

  g_game = sc_game_from_filename (path);
  if (!g_game)
    {
      fprintf (stderr, "precedence_test: cannot load %s\n", path);
      return 1;
    }

  printf ("loaded %s\n", path);
  sc_interpret_game (g_game);
  sc_free_game (g_game);

  printf ("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

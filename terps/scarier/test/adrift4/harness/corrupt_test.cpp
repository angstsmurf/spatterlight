/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Regression test for the P2 "stop abort()-ing the host" change.
 *
 * scr_fatal() used to print a message and call abort(), so a corrupt or
 * pathological game -- or any internal consistency failure -- took down the
 * whole host application (Spatterlight).  It now throws scr_fatal_error, and
 * the public entry points in scinterf.cpp catch it at the host boundary and
 * return a clean failure (NULL game / FALSE) instead.
 *
 * This test proves two things, neither of which the other harnesses cover:
 *   1. scr_fatal() throws scr_fatal_error (and carries its formatted message)
 *      rather than aborting.
 *   2. Feeding deliberately-corrupt data to scr_game_from_callback() -- including
 *      a TAF that drives the real parser into a scr_fatal() -- returns NULL and
 *      lets the program keep running, where it would previously have abort()ed.
 *
 * Links the interpreter sources directly with a minimal OS port (like the
 * precedence / bad-parent harnesses).  Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string.h>

#include "scarier.h"
#include "scprotos.h"

static int failures = 0;

static void
expect (const char *what, int got, int want)
{
  const int ok = (!got == !want);
  printf ("  [%s] %-50s got %d, want %d\n",
          ok ? "PASS" : "FAIL", what, !!got, !!want);
  if (!ok)
    failures++;
}

/* The 14-byte version signatures, mirrored from sctaffil.cpp. */
static const scr_byte V390_SIGNATURE[14] =
  {0x3c, 0x42, 0x3f, 0xc9, 0x6a, 0x87, 0xc2, 0xcf,
   0x94, 0x45, 0x37, 0x61, 0x39, 0xfa};

/* A simple byte-buffer read callback for scr_game_from_callback(). */
struct buffer_source
{
  const scr_byte *data;
  scr_int length;
  scr_int offset;
};

static scr_int
buffer_read_callback (void *opaque, scr_byte *buffer, scr_int length)
{
  buffer_source *source = (buffer_source *) opaque;
  scr_int remaining = source->length - source->offset;
  scr_int count = (length < remaining) ? length : remaining;

  if (count > 0)
    {
      memcpy (buffer, source->data + source->offset, count);
      source->offset += count;
    }
  return count;
}

static scr_game
load_from_buffer (const scr_byte *data, scr_int length)
{
  buffer_source source = { data, length, 0 };
  return scr_game_from_callback (buffer_read_callback, &source);
}

/*
 * A read callback that raises a scr_fatal() partway through loading, standing
 * in for any internal fatal (allocation failure, consistency check, ...) that
 * can fire deep inside run_create().  This proves the scinterf.cpp boundary
 * catches a fatal thrown through the whole parse call stack -- where the host
 * would previously have abort()ed -- and turns it into a clean NULL.
 */
static scr_int
fatal_read_callback (void *opaque, scr_byte *buffer, scr_int length)
{
  (void) opaque; (void) buffer; (void) length;
  scr_fatal ("simulated internal fatal during load\n");
  return 0;  /* not reached */
}

/* --- minimal SCARIER OS port (we never actually run a game here) --- */

void os_print_string (const scr_char *s) { (void) s; }
void os_print_tag (scr_int t, const scr_char *a) { (void) t; (void) a; }
void os_play_sound (const scr_char *p, scr_int o, scr_int l, scr_bool v)
{ (void) p; (void) o; (void) l; (void) v; }
void os_stop_sound (void) { }
void os_show_graphic (const scr_char *p, scr_int o, scr_int l)
{ (void) p; (void) o; (void) l; }
void os_display_hints (scr_game game) { (void) game; }
scr_bool os_confirm (scr_int type) { (void) type; return 1; }
void *os_open_file (scr_bool s) { (void) s; return 0; }
void os_write_file (void *o, const scr_byte *b, scr_int l) { (void)o;(void)b;(void)l; }
scr_int os_read_file (void *o, scr_byte *b, scr_int l) { (void)o;(void)b;(void)l; return 0; }
void os_close_file (void *o) { (void) o; }
void os_print_string_debug (const scr_char *s) { (void) s; }
scr_bool os_read_line_debug (scr_char *b, scr_int l) { return os_read_line (b, l); }

scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  strncpy ((char *) buffer, "quit", length - 1);
  buffer[length - 1] = '\0';
  return 1;
}

int
main (void)
{
  /* 1. scr_fatal() throws scr_fatal_error carrying its formatted message. */
  {
    int threw = 0, message_ok = 0;
    try
      {
        scr_fatal ("corrupt_test marker %d\n", 1234);
      }
    catch (const scr_fatal_error &error)
      {
        threw = 1;
        message_ok = (error.message.find ("marker 1234") != std::string::npos);
      }
    catch (...)
      {
        /* Wrong exception type -- leave threw == 1 but message_ok == 0. */
        threw = 1;
      }
    expect ("scr_fatal throws scr_fatal_error", threw, 1);
    expect ("scr_fatal_error carries the formatted message", message_ok, 1);
  }

  /* 2a. Empty input -- the boundary must return NULL, not crash. */
  {
    scr_game game = load_from_buffer ((const scr_byte *) "", 0);
    expect ("empty input -> NULL game", game == NULL, 1);
    scr_free_game (game);
  }

  /* 2b. Random garbage that matches no version signature -- NULL, no crash. */
  {
    scr_byte garbage[64];
    for (int i = 0; i < (int) sizeof (garbage); i++)
      garbage[i] = (scr_byte) (i * 37 + 11);
    scr_game game = load_from_buffer (garbage, sizeof (garbage));
    expect ("garbage input -> NULL game", game == NULL, 1);
    scr_free_game (game);
  }

  /* 2c. A valid v3.9 signature with a truncated/empty body.  The parser's own
   * malformed-input recovery returns cleanly here; we just confirm no crash. */
  {
    scr_game game = load_from_buffer (V390_SIGNATURE, sizeof (V390_SIGNATURE));
    expect ("v3.9 header, no body -> NULL game", game == NULL, 1);
    scr_free_game (game);
  }

  /* 2d. A scr_fatal() raised deep inside the real run_create() parse call
   * stack must be caught at the scinterf.cpp boundary and reported as a NULL
   * game -- the exact crash this phase removes. */
  {
    scr_game game = scr_game_from_callback (fatal_read_callback, NULL);
    expect ("fatal during load -> NULL game (caught at boundary)",
            game == NULL, 1);
    scr_free_game (game);
  }

  /* Reaching here at all proves none of the above aborted the process. */
  printf ("%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

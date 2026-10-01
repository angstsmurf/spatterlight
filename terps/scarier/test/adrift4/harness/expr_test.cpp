/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Regression test for the ADRIFT 4 expression evaluator (scexpr.cpp) and
 * pattern tokenizer (scparser.cpp) on hostile input:
 *   - nesting past the old 32-deep values stack used to scr_fatal() and end
 *     the game; it now evaluates, and only a pathological depth fails, as a
 *     parse error;
 *   - integer overflow wraps instead of being undefined behaviour, and the
 *     division rounding is unchanged for ordinary values;
 *   - a '%' that opens no %var% in a task pattern used to leave a stale
 *     token behind and walk past the end of the pattern.
 * Run under `make -f Makefile.headless sanitize` the last two are ASan/UBSan
 * checks as much as value checks.
 *
 * Any game will do for the variables set; pass the .taf path as argv[1].
 * Exits 0 on success, 1 on any mismatch.
 */
#include <stdio.h>
#include <string>

#include "scarier.h"
#include "scprotos.h"

static int failures = 0;

/* The game is never interpreted, so there is no input to read.
   The do-nothing stubs live in harness_os_stubs.cpp; os_read_line is ours. */
scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  (void) buffer;
  (void) length;
  return FALSE;
}

static void
expect_value (scr_var_setref_t vars, const std::string &expr, scr_int want)
{
  scr_int got = 0;
  scr_bool ok = expr_eval_numeric_expression (expr.c_str (), vars, &got);
  if (!ok || got != want)
    {
      printf ("  FAIL: %.60s -> %s %ld, want %ld\n", expr.c_str (),
              ok ? "value" : "error", got, want);
      failures++;
    }
}

static void
expect_error (scr_var_setref_t vars, const std::string &expr)
{
  scr_int got = 0;
  if (expr_eval_numeric_expression (expr.c_str (), vars, &got))
    {
      printf ("  FAIL: %.60s -> %ld, want a parse error\n", expr.c_str (), got);
      failures++;
    }
}

static void
expect_match (scr_gameref_t game, const char *pattern, const char *string,
              scr_bool want)
{
  if (!uip_match (pattern, string, game) != !want)
    {
      printf ("  FAIL: pattern \"%.40s\" vs \"%s\", want %s\n",
              pattern, string, want ? "match" : "no match");
      failures++;
    }
}

int
main (int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "precedence_test.taf";
  scr_game game;
  scr_gameref_t gs;
  scr_var_setref_t vars;
  std::string nested, deep, pattern;
  int i;

  game = scr_game_from_filename (path);
  if (!game)
    {
      fprintf (stderr, "expr_test: cannot load %s\n", path);
      return 1;
    }
  gs = (scr_gameref_t) game;
  vars = gs_get_vars (gs);

  /* Ordinary arithmetic and Adrift's asymmetric division rounding. */
  expect_value (vars, "1+2*3", 7);
  expect_value (vars, "7/2", 4);
  expect_value (vars, "-7/2", -3);
  expect_value (vars, "7/-2", -3);
  expect_value (vars, "-7/-2", 4);
  expect_value (vars, "5/2", 3);
  expect_value (vars, "-5/2", -2);
  expect_value (vars, "-7 mod 2", -1);
  expect_value (vars, "7 mod -2", 1);
  expect_value (vars, "2^10", 1024);
  expect_value (vars, "(-1)^-3", -1);

  /* Overflow wraps (two's complement), with no undefined behaviour. */
  expect_value (vars, "9223372036854775807+1", (scr_int) (-9223372036854775807L - 1));
  expect_value (vars, "-(-9223372036854775807-1)", (scr_int) (-9223372036854775807L - 1));
  expect_value (vars, "(-9223372036854775807-1)/-1", (scr_int) (-9223372036854775807L - 1));
  expect_value (vars, "(-9223372036854775807-1) mod -1", 0);
  expect_value (vars, "2^62*4", 0);

  /* 40 levels of nesting: beyond the old 32-entry stack, still evaluated. */
  for (i = 0; i < 40; i++)
    nested += "1+(";
  nested += "1";
  nested += std::string (40, ')');
  expect_value (vars, nested, 41);

  /* A pathological depth is a parse error, not a crash or a fatal. */
  deep = std::string (100000, '(') + "1";
  expect_error (vars, deep);
  expect_value (vars, "2+2", 4);

  /* Stray '%' in a pattern: a trailing one, an unclosed %var, a long run. */
  expect_match (gs, "get 50%", "get 50", FALSE);
  expect_match (gs, "get %foo", "get", FALSE);
  pattern = std::string (120, 'a') + " abcdefghij%";
  expect_match (gs, pattern.c_str (), "look", FALSE);
  expect_match (gs, "get box", "get box", TRUE);
  expect_match (gs, "get {the} box", "get the box", TRUE);

  scr_free_game (game);
  printf ("expr_test: %s, %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}

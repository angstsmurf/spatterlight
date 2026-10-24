/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

/*
 * styles_census: print the Glk style census and assignment the frontend
 * would make for each ADRIFT 4 or ADRIFT 5 game named
 * (glk/os_glk_styles.cpp), without running it.  One block per game:
 *
 *   game <file> strings <n> combos <n> spares <used>/<free> overflow <n>
 *     <rank> <combo> chars <n> intro <n|-> -> <style>[*]
 *
 * '*' marks a state that has a style of its own rather than a fallback.
 * With -s, only the summary line is printed.
 */

#include <stdio.h>
#include <string.h>

#include "core/scarier.h"
#include "adrift5/a5model.h"
#include "glk/os_glk_styles.h"

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
report (const char *path, scr_bool summary_only)
{
  FILE *stream = fopen (path, "rb");
  scr_game game;
  a5_adventure_t *adventure = nullptr;
  gsc_style_census_t census;
  gsc_style_table_t table;
  size_t index_;
  int spares = 0, overflow = 0;

  if (!stream)
    {
      printf ("game %s unreadable\n", path);
      return;
    }
  game = scr_game_from_stream (stream);
  fclose (stream);
  if (!game)
    adventure = a5model_load (path);
  if (!game && !adventure)
    {
      printf ("game %s unloadable\n", path);
      return;
    }

  if (game)
    gsc_style_census_a4 (game, &census);
  else
    gsc_style_census_a5 (adventure->root, adventure->introduction,
                         adventure->title, &census);
  gsc_style_assign (&census, &table);

  for (index_ = 0; index_ < GSC_STYLE_COUNT; index_++)
    spares += !table.natural[index_];
  for (index_ = 0; index_ < census.rows.size (); index_++)
    {
      const gsc_style_combo_t combo = census.rows[index_].combo;
      const scr_int style = gsc_style_lookup (&table, combo);

      overflow += (table.combo[style] != combo);
    }

  printf ("game %s strings %lu combos %lu spares %d overflow %d\n", path,
          census.strings, (unsigned long) census.rows.size (), spares,
          overflow);
  if (!summary_only)
    {
      for (index_ = 0; index_ < census.rows.size (); index_++)
        {
          const gsc_style_census_row_t &row = census.rows[index_];
          const scr_int style = gsc_style_lookup (&table, row.combo);
          char name[64], intro[32];

          gsc_style_combo_describe (row.combo, name, sizeof (name));
          if (row.intro_characters > 0)
            snprintf (intro, sizeof (intro), "%lu", row.intro_characters);
          else
            snprintf (intro, sizeof (intro), "-");
          printf ("  %2lu %-24s chars %7lu intro %5s -> %s%s\n",
                  (unsigned long) index_ + 1, name, row.characters, intro,
                  gsc_style_name (style),
                  table.combo[style] == row.combo ? "*" : "");
        }
    }

  if (game)
    scr_free_game (game);
  else
    a5model_free (adventure);
}

int
main (int argc, const char *argv[])
{
  scr_bool summary_only = FALSE;
  int index_;

  for (index_ = 1; index_ < argc; index_++)
    {
      if (strcmp (argv[index_], "-s") == 0)
        summary_only = TRUE;
      else
        report (argv[index_], summary_only);
    }
  return 0;
}

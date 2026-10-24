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
 * os_glk_styles.cpp: the style census and assignment; see os_glk_styles.h.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>

#include "os_glk_styles.h"

/* The deepest <font> nesting the frontend tracks, as in os_glk_internal.h. */
static const int GSC_STYLE_MAX_NESTING = 32;

/* The property path of the introduction. */
static const char *const GSC_STYLE_INTRO_PATH = "Header/StartupText";

/*
 * The game name, which the engine prints above the introduction wrapped in
 * markup of its own (run_main_intro(): "<font size=14><c>" ... "</c></font>").
 * The census wraps it the same way, so the title's state is counted as the
 * introduction text it is.
 */
static const char *const GSC_STYLE_TITLE_PATH = "Globals/GameName";


/*
 * gsc_style_combo()
 * gsc_style_combo_step()
 * gsc_style_combo_describe()
 *
 * Build an attribute state, read its size step back, and spell it out for a
 * report ("bold+centre size+3").
 */
gsc_style_combo_t
gsc_style_combo (scr_bool bold, scr_bool italic, scr_bool monospaced,
                 scr_bool center, scr_bool right, scr_bool standin,
                 scr_int points)
{
  gsc_style_combo_t combo = 0;
  scr_int step;

  if (bold)
    combo |= GSC_COMBO_BOLD;
  if (italic)
    combo |= GSC_COMBO_ITALIC;
  if (monospaced)
    combo |= GSC_COMBO_MONOSPACED;

  /* Alignment is one state in the frontend; right is what it says last. */
  if (right)
    combo |= GSC_COMBO_RIGHT;
  else if (center)
    combo |= GSC_COMBO_CENTER;

  /*
   * Two points to a step, truncating toward the default, so 13pt is still
   * the default size and 11pt is too; the clamp is the range a Size hint
   * can sensibly ask for.
   */
  step = (points - GSC_STYLE_DEFAULT_POINTS) / 2;
  step = std::max<scr_int> (GSC_STYLE_MIN_STEP,
                            std::min<scr_int> (GSC_STYLE_MAX_STEP, step));
  combo |= ((gsc_style_combo_t) step << GSC_COMBO_SIZE_SHIFT)
           & GSC_COMBO_SIZE_MASK;

  /* <c> with the colours off is Emphasized, but only on plain text. */
  if (standin && combo == 0)
    combo = GSC_COMBO_ITALIC;

  return combo;
}

scr_int
gsc_style_combo_step (gsc_style_combo_t combo)
{
  return (signed char) ((combo & GSC_COMBO_SIZE_MASK) >> GSC_COMBO_SIZE_SHIFT);
}

void
gsc_style_combo_describe (gsc_style_combo_t combo, char *buffer, size_t length)
{
  static const struct
  {
    gsc_style_combo_t bit;
    const char *name;
  } names[] = {
    {GSC_COMBO_BOLD, "bold"}, {GSC_COMBO_ITALIC, "italic"},
    {GSC_COMBO_MONOSPACED, "mono"}, {GSC_COMBO_CENTER, "centre"},
    {GSC_COMBO_RIGHT, "right"}
  };
  const scr_int step = gsc_style_combo_step (combo);
  size_t used = 0;
  size_t index_;

  buffer[0] = '\0';
  for (index_ = 0; index_ < sizeof (names) / sizeof (names[0]); index_++)
    {
      if (combo & names[index_].bit)
        used += snprintf (buffer + used, length > used ? length - used : 0,
                          "%s%s", used > 0 ? "+" : "", names[index_].name);
    }
  if (step != 0)
    used += snprintf (buffer + used, length > used ? length - used : 0,
                      "%ssize%+ld", used > 0 ? " " : "", step);
  if (used == 0)
    snprintf (buffer, length, "plain");
}


/*
 * gsc_style_face_is_monospaced()
 * gsc_style_apply_font_size()
 *
 * The face= and size= parts of a lowercased <font> argument.  There may be
 * plenty of monospaced fonts, but we do only courier and terminal.  Sizes are
 * absolute, or relative to the enclosing font with a sign.
 */
scr_bool
gsc_style_face_is_monospaced (const char *face)
{
  return strncmp (face, "face=\"courier\"", 14) == 0
         || strncmp (face, "face=\"terminal\"", 15) == 0;
}

scr_int
gsc_style_apply_font_size (const char *size, scr_int points)
{
  scr_uint value;

  if (strncmp (size, "size=+", 6) == 0
      && sscanf (size, "size=+%lu", &value) == 1)
    return points + value;
  else if (strncmp (size, "size=-", 6) == 0
           && sscanf (size, "size=-%lu", &value) == 1)
    return points - value;
  else if (sscanf (size, "size=%lu", &value) == 1)
    return value;
  return points;
}


/*
 * gsc_style_table_natural()
 * gsc_style_lookup()
 * gsc_style_name()
 *
 * The assignment today's fixed precedence amounts to, the print-time lookup
 * with its fallback chain, and the styles' names for reports.
 */
void
gsc_style_table_natural (gsc_style_table_t *table)
{
  static const struct
  {
    scr_int style;
    gsc_style_combo_t combo;
  } naturals[] = {
    {GSC_STYLE_NORMAL, 0},
    {GSC_STYLE_EMPHASIZED, GSC_COMBO_ITALIC},
    {GSC_STYLE_PREFORMATTED, GSC_COMBO_MONOSPACED},
    {GSC_STYLE_SUBHEADER, GSC_COMBO_BOLD},
    {GSC_STYLE_ALERT, GSC_COMBO_BOLD | GSC_COMBO_ITALIC},
    {GSC_STYLE_NOTE, GSC_COMBO_RIGHT},
    {GSC_STYLE_USER1, GSC_COMBO_CENTER},
    {GSC_STYLE_USER2, GSC_COMBO_CENTER | GSC_COMBO_BOLD}
  };
  size_t index_;

  for (index_ = 0; index_ < GSC_STYLE_COUNT; index_++)
    {
      table->assigned[index_] = FALSE;
      table->natural[index_] = TRUE;
      table->combo[index_] = 0;
    }
  for (index_ = 0; index_ < sizeof (naturals) / sizeof (naturals[0]); index_++)
    {
      table->assigned[naturals[index_].style] = TRUE;
      table->combo[naturals[index_].style] = naturals[index_].combo;
    }
}

static scr_int
gsc_style_find (const gsc_style_table_t *table, gsc_style_combo_t combo)
{
  scr_int style;

  for (style = 0; style < GSC_STYLE_COUNT; style++)
    {
      if (table->assigned[style] && table->combo[style] == combo)
        return style;
    }
  return -1;
}

scr_int
gsc_style_lookup (const gsc_style_table_t *table, gsc_style_combo_t combo)
{
  /*
   * Each step drops attributes and none adds one, so text never shows an
   * attribute it didn't ask for: first the size, a step at a time toward the
   * default so a size the game uses a little of lands on a near size it uses
   * more; then italic, then bold (before the alignment, so right-aligned bold
   * stays right-aligned, as it is today), then the alignment, then the face.
   * Normal is always assigned, so the walk ends there at the latest.
   */
  static const gsc_style_combo_t drops[] = {
    GSC_COMBO_ITALIC, GSC_COMBO_BOLD, GSC_COMBO_CENTER | GSC_COMBO_RIGHT,
    GSC_COMBO_MONOSPACED
  };
  const gsc_style_combo_t flags = combo & GSC_COMBO_FLAGS;
  scr_int step, style;
  size_t index_;

  for (step = gsc_style_combo_step (combo); ; step += (step < 0) - (step > 0))
    {
      style = gsc_style_find (table, flags
                              | (((gsc_style_combo_t) step
                                  << GSC_COMBO_SIZE_SHIFT)
                                 & GSC_COMBO_SIZE_MASK));
      if (style >= 0)
        return style;
      if (step == 0)
        break;
    }

  combo = flags;
  for (index_ = 0; index_ < sizeof (drops) / sizeof (drops[0]); index_++)
    {
      combo &= ~drops[index_];
      style = gsc_style_find (table, combo);
      if (style >= 0)
        return style;
    }
  return GSC_STYLE_NORMAL;
}

const char *
gsc_style_name (scr_int style)
{
  static const char *const names[GSC_STYLE_COUNT] = {
    "Normal", "Emphasized", "Preformatted", "Header", "Subheader", "Alert",
    "Note", "BlockQuote", "Input", "User1", "User2"
  };

  return style >= 0 && style < GSC_STYLE_COUNT ? names[style] : "?";
}


/*
 * gsc_style_census_a4()
 *
 * Feed every string in an ADRIFT 4 game through a copy of the frontend's tag
 * state (os_glk_output.cpp, gsc_handle_font_tag() and friends): flat bold,
 * italic and underline flags, a counted <c>, a font stack of the same depth,
 * and one alignment.  Each string starts from the plain state the frontend
 * resets to at every prompt.  Text is counted in visible characters, so one
 * big title weighs what its words do rather than what its tags do.
 */
namespace {

struct gsc_style_walk_t
{
  struct font_t
  {
    scr_bool monospaced;
    scr_int points;
  };

  font_t fonts[GSC_STYLE_MAX_NESTING];
  int font_count;
  scr_bool bold, italic, underline, center, right;
  int colour;

  scr_bool in_intro;
  std::map<gsc_style_combo_t, gsc_style_census_row_t> rows;
  unsigned long strings;

  void reset ();
  font_t top () const;
  gsc_style_combo_t combo () const;
  void text (const scr_char *string);
  void tag (scr_int tag, const scr_char *argument);
};

} /* namespace */

void
gsc_style_walk_t::reset ()
{
  font_count = 0;
  bold = italic = underline = center = right = FALSE;
  colour = 0;
}

gsc_style_walk_t::font_t
gsc_style_walk_t::top () const
{
  if (font_count > 0)
    return fonts[font_count - 1];

  font_t font = {FALSE, GSC_STYLE_DEFAULT_POINTS};
  return font;
}

gsc_style_combo_t
gsc_style_walk_t::combo () const
{
  const font_t font = top ();

  /* The census takes the colours as off, the case where <c> has a style. */
  return gsc_style_combo (bold, italic || underline, font.monospaced,
                          center, right, colour > 0, font.points);
}

void
gsc_style_walk_t::text (const scr_char *string)
{
  unsigned long visible = 0;
  const scr_char *cursor;

  for (cursor = string; *cursor != '\0'; cursor++)
    {
      if (*cursor != ' ' && *cursor != '\t'
          && *cursor != '\n' && *cursor != '\r')
        visible++;
    }
  if (visible == 0)
    return;

  const gsc_style_combo_t current = combo ();
  std::map<gsc_style_combo_t, gsc_style_census_row_t>::iterator
      row = rows.find (current);
  if (row == rows.end ())
    {
      gsc_style_census_row_t fresh = {current, 0, 0};
      row = rows.insert (std::make_pair (current, fresh)).first;
    }

  row->second.characters += visible;
  if (in_intro)
    row->second.intro_characters += visible;
}

void
gsc_style_walk_t::tag (scr_int tag, const scr_char *argument)
{
  switch (tag)
    {
    case SCR_TAG_FONT:
      /* Ignored on overrun, as the frontend ignores it. */
      if (font_count < GSC_STYLE_MAX_NESTING)
        {
          font_t font = top ();
          std::string lower (argument);
          size_t index_;

          for (index_ = 0; index_ < lower.size (); index_++)
            lower[index_] = (char) tolower ((unsigned char) lower[index_]);

          const size_t face = lower.find ("face=");
          if (face != std::string::npos)
            font.monospaced = gsc_style_face_is_monospaced (lower.c_str ()
                                                            + face);
          const size_t size = lower.find ("size=");
          if (size != std::string::npos)
            font.points = gsc_style_apply_font_size (lower.c_str () + size,
                                                     font.points);
          fonts[font_count++] = font;
        }
      break;

    case SCR_TAG_ENDFONT:
      if (font_count > 0)
        font_count--;
      break;

    case SCR_TAG_BOLD:
      bold = TRUE;
      break;
    case SCR_TAG_ENDBOLD:
      bold = FALSE;
      break;
    case SCR_TAG_ITALICS:
      italic = TRUE;
      break;
    case SCR_TAG_ENDITALICS:
      italic = FALSE;
      break;
    case SCR_TAG_UNDERLINE:
      underline = TRUE;
      break;
    case SCR_TAG_ENDUNDERLINE:
      underline = FALSE;
      break;
    case SCR_TAG_COLOUR:
      colour++;
      break;
    case SCR_TAG_ENDCOLOUR:
      if (colour > 0)
        colour--;
      break;

    /* One alignment, as in gsc_set_alignment(). */
    case SCR_TAG_CENTER:
    case SCR_TAG_ENDCENTER:
      center = (tag == SCR_TAG_CENTER);
      right = FALSE;
      break;
    case SCR_TAG_RIGHT:
    case SCR_TAG_ENDRIGHT:
      right = (tag == SCR_TAG_RIGHT);
      center = FALSE;
      break;

    default:
      break;
    }
}

static void
gsc_style_walk_begin (const scr_char *path, void *opaque)
{
  gsc_style_walk_t *walk = (gsc_style_walk_t *) opaque;

  walk->reset ();
  walk->in_intro = strcmp (path, GSC_STYLE_INTRO_PATH) == 0;
  walk->strings++;

  if (strcmp (path, GSC_STYLE_TITLE_PATH) == 0)
    {
      walk->in_intro = TRUE;
      walk->tag (SCR_TAG_FONT, "size=14");
      walk->tag (SCR_TAG_COLOUR, "");
    }
}

static void
gsc_style_walk_text (const scr_char *text, void *opaque)
{
  ((gsc_style_walk_t *) opaque)->text (text);
}

static void
gsc_style_walk_tag (scr_int tag, const scr_char *argument, void *opaque)
{
  ((gsc_style_walk_t *) opaque)->tag (tag, argument);
}

/*
 * The order the assignment hands out styles in: by how much text a state
 * covers, the simpler state first on a tie, and the combo value last so the
 * order never depends on the map.  Introduction text counts many times over,
 * it being the first screen the player sees and its title states being the
 * reason game styles exist at all; but not so many that a ten-character
 * byline outranks a state that covers whole rooms.  Across the corpus, ranking
 * the introduction strictly first leaves twice the text without a style of
 * its own that this weight does (see TODO-styles.md, Step 2).
 */
static const unsigned long GSC_STYLE_INTRO_WEIGHT = 20;

static unsigned int
gsc_style_combo_bits (gsc_style_combo_t combo)
{
  unsigned int bits = gsc_style_combo_step (combo) != 0;

  for (combo &= GSC_COMBO_FLAGS; combo != 0; combo &= combo - 1)
    bits++;
  return bits;
}

static bool
gsc_style_rank_before (const gsc_style_census_row_t &a,
                       const gsc_style_census_row_t &b)
{
  const unsigned long a_weight = a.characters
                                 + GSC_STYLE_INTRO_WEIGHT * a.intro_characters;
  const unsigned long b_weight = b.characters
                                 + GSC_STYLE_INTRO_WEIGHT * b.intro_characters;
  const unsigned int a_bits = gsc_style_combo_bits (a.combo);
  const unsigned int b_bits = gsc_style_combo_bits (b.combo);

  if (a_weight != b_weight)
    return a_weight > b_weight;
  if (a_bits != b_bits)
    return a_bits < b_bits;
  return a.combo < b.combo;
}

void
gsc_style_census_a4 (scr_game game, gsc_style_census_t *census)
{
  gsc_style_walk_t walk;
  scr_markup_sink_t sink;
  std::map<gsc_style_combo_t, gsc_style_census_row_t>::const_iterator row;

  walk.reset ();
  walk.in_intro = FALSE;
  walk.strings = 0;

  sink.begin = gsc_style_walk_begin;
  sink.text = gsc_style_walk_text;
  sink.tag = gsc_style_walk_tag;
  sink.opaque = &walk;
  scr_game_scan_markup (game, &sink);

  census->rows.clear ();
  for (row = walk.rows.begin (); row != walk.rows.end (); ++row)
    census->rows.push_back (row->second);
  std::sort (census->rows.begin (), census->rows.end (),
             gsc_style_rank_before);
  census->strings = walk.strings;
}


/*
 * gsc_style_assign()
 *
 * Start from the natural table, then give each ranked state that has no
 * style of its own the next spare: Header and BlockQuote, which today's
 * precedence leaves to error text or doesn't use, then the natural styles
 * whose state the game never shows.  Emphasized and Preformatted are never
 * spares -- the frontend prints its own notes in Emphasized -- and Normal
 * and Input are what they are.
 */
void
gsc_style_assign (const gsc_style_census_t *census, gsc_style_table_t *table)
{
  static const scr_int fixed_spares[] = {GSC_STYLE_HEADER,
                                         GSC_STYLE_BLOCKQUOTE};
  static const scr_int natural_spares[] = {GSC_STYLE_USER1, GSC_STYLE_USER2,
                                           GSC_STYLE_NOTE, GSC_STYLE_ALERT,
                                           GSC_STYLE_SUBHEADER};
  std::vector<scr_int> spares;
  size_t index_, next;

  gsc_style_table_natural (table);

  spares.assign (fixed_spares,
                 fixed_spares + sizeof (fixed_spares) / sizeof (fixed_spares[0]));
  for (index_ = 0;
       index_ < sizeof (natural_spares) / sizeof (natural_spares[0]); index_++)
    {
      const scr_int style = natural_spares[index_];
      std::vector<gsc_style_census_row_t>::const_iterator row;
      scr_bool used = FALSE;

      for (row = census->rows.begin (); row != census->rows.end (); ++row)
        {
          if (row->combo == table->combo[style])
            used = TRUE;
        }
      if (!used)
        spares.push_back (style);
    }

  next = 0;
  for (index_ = 0; index_ < census->rows.size () && next < spares.size ();
       index_++)
    {
      const gsc_style_combo_t combo = census->rows[index_].combo;

      if (gsc_style_find (table, combo) >= 0)
        continue;

      const scr_int style = spares[next++];
      table->assigned[style] = TRUE;
      table->natural[style] = FALSE;
      table->combo[style] = combo;
    }
}

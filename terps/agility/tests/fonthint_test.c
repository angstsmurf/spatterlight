/*
 * fonthint_test.c
 *
 * Standalone verification harness for the os_glk.c paragraph reflow logic
 * that decides whether each buffered line keeps its newline (own line) or
 * is reflowed into the previous line (joined).
 *
 * The structs and the gagt_* functions below are copied VERBATIM from
 * terps/agility/os_glk.c (the grouping, metric, standout, font-hint and
 * hinted-line-render logic), so this exercises the real algorithm against
 * the exact Shades of Gray inventory output.
 *
 * Build both states:
 *   cc -DGAGT_NOFIX -o /tmp/fh_before fonthint_test.c   (pre-fix algorithm)
 *   cc            -o /tmp/fh_after  fonthint_test.c      (with Phase 4.5 fix)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#define TRUE 1
#define FALSE 0
typedef unsigned int glui32;

static int screen_width = 80;

/* ---- struct definitions (verbatim shapes from os_glk.c) ---- */

typedef struct {
  glui32 *data;
  glui32 *attributes;
  int allocation;
  int length;
} gagt_string_t;
typedef gagt_string_t *gagt_stringref_t;

typedef enum {
  HINT_NONE,
  HINT_PROPORTIONAL,
  HINT_PROPORTIONAL_NEWLINE,
  HINT_PROPORTIONAL_NEWLINE_STANDOUT,
  HINT_FIXED_WIDTH
} gagt_font_hint_t;

static const unsigned int GAGT_LINE_MAGIC = 0x5bc14482;
static const unsigned int GAGT_PARAGRAPH_MAGIC = 0xb9a2297b;

typedef struct gagt_line_s *gagt_lineref_t;
typedef struct gagt_paragraph_s *gagt_paragraphref_t;

struct gagt_line_s {
  unsigned int magic;
  gagt_string_t buffer;
  int indent;
  int outdent;
  int real_length;
  int is_blank;
  int is_hyphenated;
  gagt_paragraphref_t paragraph;
  gagt_font_hint_t font_hint;
  gagt_lineref_t next;
  gagt_lineref_t prior;
};

struct gagt_paragraph_s {
  unsigned int magic;
  gagt_lineref_t first_line;
  void *special;                 /* gagt_specialref_t in real code; NULL here */
  int line_count;
  int id;
  gagt_paragraphref_t next;
  gagt_paragraphref_t prior;
};

static gagt_lineref_t gagt_page_head = NULL, gagt_page_tail = NULL;
static gagt_paragraphref_t gagt_paragraphs_head = NULL, gagt_paragraphs_tail = NULL;

static void *gagt_malloc (size_t n) { void *p = malloc (n); assert (p); return p; }

/* ---- attribute unpack (no AGT formatting in this corpus, so all clear) ---- */
typedef struct { int blink, fixed, emphasis; } gagt_attrset_t;
static void
gagt_unpack_attributes (unsigned char packed, gagt_attrset_t *a)
{
  a->blink = a->fixed = a->emphasis = FALSE;
  (void) packed;
}

/* =================== VERBATIM from os_glk.c =================== */

static int
gagt_get_string_indent (const gagt_stringref_t buffer)
{
  int indent, index;
  indent = 0;
  for (index = 0; index < buffer->length && isspace (buffer->data[index]); index++)
    indent++;
  return indent;
}

static int
gagt_get_string_outdent (const gagt_stringref_t buffer)
{
  int outdent, index;
  outdent = 0;
  for (index = buffer->length - 1; index >= 0 && buffer->data[index] == 32; index--)
    outdent++;
  return outdent;
}

static int
gagt_get_string_real_length (const gagt_stringref_t buffer)
{
  int indent, outdent;
  indent = gagt_get_string_indent (buffer);
  outdent = gagt_get_string_outdent (buffer);
  return indent == buffer->length ? 0 : buffer->length - indent - outdent;
}

static int
gagt_is_string_blank (const gagt_stringref_t buffer)
{
  return gagt_get_string_indent (buffer) == buffer->length;
}

static int
gagt_is_string_hyphenated (const gagt_stringref_t buffer)
{
  int is_hyphenated;
  is_hyphenated = FALSE;
  if (!gagt_is_string_blank (buffer) && gagt_get_string_real_length (buffer) > 1)
    {
      int last;
      last = buffer->length - gagt_get_string_outdent (buffer) - 1;
      if (buffer->data[last] == '-')
        {
          if (isalpha (buffer->data[last - 1]))
            is_hyphenated = TRUE;
        }
    }
  return is_hyphenated;
}

static gagt_lineref_t
gagt_get_first_page_line (void) { return gagt_page_head; }

static gagt_lineref_t
gagt_get_next_page_line (const gagt_lineref_t line) { return line->next; }

static gagt_lineref_t
gagt_get_prior_page_line (const gagt_lineref_t line) { return line->prior; }

static gagt_lineref_t
gagt_find_paragraph_start (const gagt_lineref_t begin)
{
  gagt_lineref_t line, match;
  match = NULL;
  for (line = begin; line; line = gagt_get_next_page_line (line))
    if (!line->is_blank) { match = line; break; }
  return match;
}

static gagt_lineref_t
gagt_find_block_end (const gagt_lineref_t begin, int indent)
{
  gagt_lineref_t line, match;
  match = begin;
  for (line = begin; line; line = gagt_get_next_page_line (line))
    {
      if (line->is_blank || (indent > 0 && line->indent == indent))
        break;
      match = line;
    }
  return match;
}

static gagt_lineref_t
gagt_find_blank_line_block_end (const gagt_lineref_t begin)
{
  return gagt_find_block_end (begin, -1);
}

static gagt_lineref_t
gagt_find_paragraph_end (const gagt_lineref_t first_line)
{
  gagt_lineref_t second_line;
  second_line = gagt_get_next_page_line (first_line);
  if (!second_line || second_line->is_blank)
    return first_line;

  if (first_line->indent > screen_width / 4 || second_line->indent > screen_width / 4)
    return gagt_find_blank_line_block_end (second_line);

  else if (first_line->indent > second_line->indent)
    return gagt_find_block_end (second_line, first_line->indent);

  else if (second_line->indent > first_line->indent)
    {
      gagt_lineref_t third_line;
      third_line = gagt_get_next_page_line (second_line);
      if (!third_line || third_line->is_blank)
        return second_line;
      if (second_line->indent > screen_width / 4 || third_line->indent > screen_width / 4)
        return gagt_find_blank_line_block_end (third_line);
      else if (second_line->indent > third_line->indent)
        return gagt_find_block_end (third_line, second_line->indent);
      else
        return gagt_find_blank_line_block_end (third_line);
    }
  else
    {
      assert (second_line->indent == first_line->indent);
      return gagt_find_blank_line_block_end (second_line);
    }
}

static void
gagt_paragraph_page (void)
{
  gagt_lineref_t start;
  start = gagt_find_paragraph_start (gagt_get_first_page_line ());
  while (start)
    {
      gagt_paragraphref_t paragraph;
      gagt_lineref_t end, line;
      paragraph = gagt_malloc (sizeof (*paragraph));
      paragraph->magic = GAGT_PARAGRAPH_MAGIC;
      paragraph->first_line = start;
      paragraph->special = NULL;
      paragraph->line_count = 1;
      paragraph->id = gagt_paragraphs_tail ? gagt_paragraphs_tail->id + 1 : 0;
      paragraph->next = NULL;
      paragraph->prior = gagt_paragraphs_tail;
      if (gagt_paragraphs_head && gagt_paragraphs_tail)
        gagt_paragraphs_tail->next = paragraph;
      else
        gagt_paragraphs_head = paragraph;
      gagt_paragraphs_tail = paragraph;
      end = gagt_find_paragraph_end (start);
      for (line = start; line != end; line = gagt_get_next_page_line (line))
        { line->paragraph = paragraph; paragraph->line_count++; }
      if (end)
        end->paragraph = paragraph;
      line = gagt_get_next_page_line (end);
      if (line)
        start = gagt_find_paragraph_start (line);
      else
        start = NULL;
    }
}

static gagt_paragraphref_t
gagt_get_first_paragraph (void) { return gagt_paragraphs_head; }

static gagt_paragraphref_t
gagt_get_next_paragraph (const gagt_paragraphref_t paragraph) { return paragraph->next; }

static gagt_lineref_t
gagt_get_first_paragraph_line (const gagt_paragraphref_t paragraph)
{ return paragraph->first_line; }

static gagt_lineref_t
gagt_get_next_paragraph_line (const gagt_lineref_t line)
{
  gagt_lineref_t next_line;
  next_line = gagt_get_next_page_line (line);
  if (next_line && next_line->paragraph == line->paragraph)
    return next_line;
  else
    return NULL;
}

static gagt_lineref_t
gagt_get_prior_paragraph_line (const gagt_lineref_t line)
{
  gagt_lineref_t prior_line;
  prior_line = gagt_get_prior_page_line (line);
  if (prior_line && prior_line->paragraph == line->paragraph)
    return prior_line;
  else
    return NULL;
}

static const int GAGT_THRESHOLD = 4, GAGT_COMMON_THRESHOLD = 8;
static const char *const GAGT_COMMON_PUNCTUATION = ".!?";

static int
gagt_line_is_standout (const gagt_lineref_t line)
{
  int index, all_formatted, upper_count, lower_count;
  all_formatted = TRUE;
  upper_count = lower_count = 0;
  for (index = line->indent; index < line->buffer.length - line->outdent; index++)
    {
      gagt_attrset_t attribute_set;
      unsigned char character;
      gagt_unpack_attributes (line->buffer.attributes[index], &attribute_set);
      character = line->buffer.data[index];
      if (!(attribute_set.blink || attribute_set.fixed || attribute_set.emphasis))
        all_formatted = FALSE;
      if (islower (character))
        lower_count++;
      else if (isupper (character))
        upper_count++;
    }
  return all_formatted || (upper_count > 0 && lower_count == 0);
}

static void
gagt_set_font_hint_proportional (gagt_lineref_t line)
{
  if (line->font_hint == HINT_NONE)
    line->font_hint = HINT_PROPORTIONAL;
}

static void
gagt_set_font_hint_proportional_newline (gagt_lineref_t line)
{
  if (line->font_hint == HINT_NONE || line->font_hint == HINT_PROPORTIONAL)
    {
      if (gagt_line_is_standout (line))
        line->font_hint = HINT_PROPORTIONAL_NEWLINE_STANDOUT;
      else
        line->font_hint = HINT_PROPORTIONAL_NEWLINE;
    }
}

static void
gagt_set_font_hint_fixed_width (gagt_lineref_t line)
{
  if (line->font_hint == HINT_NONE
      || line->font_hint == HINT_PROPORTIONAL
      || line->font_hint == HINT_PROPORTIONAL_NEWLINE
      || line->font_hint == HINT_PROPORTIONAL_NEWLINE_STANDOUT)
    line->font_hint = HINT_FIXED_WIDTH;
}

static void
gagt_assign_paragraph_font_hints (const gagt_paragraphref_t paragraph)
{
  static int is_initialized = FALSE;
  static int threshold[UCHAR_MAX + 1];
  gagt_lineref_t line, first_line;
  int is_table, in_list;
  assert (paragraph);

  if (!is_initialized)
    {
      int character;
      for (character = 0; character <= UCHAR_MAX; character++)
        if (ispunct (character))
          threshold[character] = strchr (GAGT_COMMON_PUNCTUATION, character)
                                 ? GAGT_COMMON_THRESHOLD : GAGT_THRESHOLD;
      is_initialized = TRUE;
    }

  first_line = gagt_get_first_paragraph_line (paragraph);
  assert (first_line);

  /* Phase 1 */
  if (gagt_get_first_paragraph () == paragraph
      && !gagt_get_next_paragraph (paragraph)
      && !gagt_get_next_paragraph_line (first_line))
    {
      gagt_set_font_hint_proportional_newline (first_line);
      return;
    }

  /* Phase 2 */
  is_table = FALSE;
  for (line = first_line; line && !is_table; line = gagt_get_next_paragraph_line (line))
    {
      int index, counts[UCHAR_MAX + 1], total_counts;
      memset (counts, 0, sizeof (counts));
      total_counts = 0;
      for (index = line->indent;
           index < line->buffer.length - line->outdent && !is_table; index++)
        {
          int character = line->buffer.data[index];
          if (ispunct (character))
            {
              counts[character]++;
              total_counts++;
              is_table = (counts[character] >= threshold[character]);
            }
          else if (total_counts > 0)
            { memset (counts, 0, sizeof (counts)); total_counts = 0; }
        }
    }

  /* Phase 3 */
  if (!is_table)
    {
      for (line = first_line; line && !is_table; line = gagt_get_next_paragraph_line (line))
        {
          int index, count = 0;
          for (index = line->indent;
               index < line->buffer.length - line->outdent && !is_table; index++)
            {
              int character = line->buffer.data[index];
              if (isspace (character)) { count++; is_table = (count >= GAGT_THRESHOLD); }
              else count = 0;
            }
        }
    }

  if (is_table && gagt_get_next_paragraph_line (first_line))
    {
      for (line = first_line; line; line = gagt_get_next_paragraph_line (line))
        gagt_set_font_hint_fixed_width (line);
      return;
    }

  /* Phase 4 */
  if (gagt_line_is_standout (first_line)
      || first_line->real_length < screen_width / 2)
    {
      gagt_set_font_hint_proportional_newline (first_line);
      first_line = gagt_get_next_paragraph_line (first_line);
      if (!first_line)
        return;
    }

#ifndef GAGT_NOFIX
  /* Phase 4.5 -- preserve line breaks in indented lists. */
  {
    int is_list = FALSE;
    for (line = gagt_get_next_paragraph_line (first_line);
         line; line = gagt_get_next_paragraph_line (line))
      if (line->indent > first_line->indent) { is_list = TRUE; break; }
    if (is_list)
      {
        for (line = first_line; line; line = gagt_get_next_paragraph_line (line))
          gagt_set_font_hint_proportional_newline (line);
        return;
      }
  }
#endif

  /* Phase 5 */
  in_list = FALSE;
  for (line = first_line; line; line = gagt_get_next_paragraph_line (line))
    {
      gagt_lineref_t next_line;
      next_line = gagt_get_next_paragraph_line (line);
      if (!next_line)
        { gagt_set_font_hint_proportional_newline (line); continue; }
      if (next_line->indent > first_line->indent)
        { gagt_set_font_hint_proportional_newline (line); in_list = TRUE; }
      else
        {
          if (in_list) gagt_set_font_hint_proportional_newline (line);
          else gagt_set_font_hint_proportional (line);
          in_list = FALSE;
        }
    }

  /* Phase 6 */
  if (gagt_line_is_standout (first_line))
    gagt_set_font_hint_proportional_newline (first_line);
  for (line = gagt_get_next_paragraph_line (first_line); line;
       line = gagt_get_next_paragraph_line (line))
    if (gagt_line_is_standout (line))
      {
        gagt_lineref_t prior_line;
        gagt_set_font_hint_proportional_newline (line);
        prior_line = gagt_get_prior_paragraph_line (line);
        gagt_set_font_hint_proportional_newline (prior_line);
      }

  /* Phase 7 */
  if (gagt_get_next_paragraph_line (first_line))
    {
      gagt_lineref_t next_line = gagt_get_next_paragraph_line (first_line);
      if (first_line->real_length < screen_width / 2
          && next_line->real_length > screen_width * 3 / 4)
        gagt_set_font_hint_proportional_newline (first_line);
    }

  /* Phase 8 */
  if (gagt_get_next_paragraph_line (first_line))
    {
      int all_short = TRUE;
      for (line = first_line; line; line = gagt_get_next_paragraph_line (line))
        if (line->real_length >= screen_width / 2) { all_short = FALSE; break; }
      if (all_short)
        for (line = first_line; line; line = gagt_get_next_paragraph_line (line))
          gagt_set_font_hint_proportional_newline (line);
    }
}

/* =================== end verbatim =================== */

/* ---- test scaffolding ---- */

static gagt_lineref_t
make_line (const char *text)
{
  gagt_lineref_t line = gagt_malloc (sizeof (*line));
  int len = (int) strlen (text), i;
  line->magic = GAGT_LINE_MAGIC;
  line->buffer.length = len;
  line->buffer.allocation = len;
  line->buffer.data = gagt_malloc ((len + 1) * sizeof (glui32));
  line->buffer.attributes = gagt_malloc ((len + 1) * sizeof (glui32));
  for (i = 0; i < len; i++)
    { line->buffer.data[i] = (unsigned char) text[i]; line->buffer.attributes[i] = 0; }
  line->indent = gagt_get_string_indent (&line->buffer);
  line->outdent = gagt_get_string_outdent (&line->buffer);
  line->real_length = gagt_get_string_real_length (&line->buffer);
  line->is_blank = gagt_is_string_blank (&line->buffer);
  line->is_hyphenated = gagt_is_string_hyphenated (&line->buffer);
  line->paragraph = NULL;
  line->font_hint = HINT_NONE;
  line->next = NULL;
  line->prior = gagt_page_tail;
  if (gagt_page_head) gagt_page_tail->next = line;
  else gagt_page_head = line;
  gagt_page_tail = line;
  return line;
}

static void
print_trimmed (const gagt_lineref_t line)
{
  int i;
  for (i = line->indent; i < line->buffer.length - line->outdent; i++)
    putchar ((int) line->buffer.data[i]);
}

/* Render the way gagt_display_auto + gagt_display_hinted_line would:
 * HINT_PROPORTIONAL emits a space (joins next line); the *_NEWLINE hints
 * and end-of-paragraph emit a newline. */
static void
render (void)
{
  gagt_paragraphref_t paragraph;
  for (paragraph = gagt_get_first_paragraph (); paragraph;
       paragraph = gagt_get_next_paragraph (paragraph))
    {
      gagt_lineref_t line;
      for (line = gagt_get_first_paragraph_line (paragraph); line;
           line = gagt_get_next_paragraph_line (line))
        {
          print_trimmed (line);
          if (line->font_hint == HINT_PROPORTIONAL)
            putchar (' ');          /* joined to next line */
          else
            putchar ('\n');         /* own line */
        }
      putchar ('\n');               /* paragraph break */
    }
}

static void
reset (void)
{
  gagt_page_head = gagt_page_tail = NULL;
  gagt_paragraphs_head = gagt_paragraphs_tail = NULL;
}

static void
run (const char *title)
{
  gagt_paragraphref_t p;
  gagt_paragraph_page ();
  for (p = gagt_get_first_paragraph (); p; p = gagt_get_next_paragraph (p))
    gagt_assign_paragraph_font_hints (p);
  printf ("--- %s ---\n", title);
  render ();
  reset ();
}

int
main (void)
{
#ifdef GAGT_NOFIX
  printf ("=== BEFORE (current os_glk.c reflow) ===\n\n");
#else
  printf ("=== AFTER (with Phase 4.5 list fix) ===\n\n");
#endif

  /* The reported bug: Shades of Gray inventory.  print_obj() indents 3
   * spaces per nesting level; the wallet's contained items are one deeper. */
  make_line ("You are carrying the following:");
  make_line ("   leather wallet");
  make_line ("      There is a yellowed news clipping. (in the wallet)");
  make_line ("      There is color photograph of a pretty girl. (in the wallet)");
  make_line ("      You find a silver foil wrapper. (in the wallet)");
  make_line ("   sharp switchblade");
  make_line ("   two dollars");
  run ("inventory (should be one item per line)");

  /* Regression: ordinary hard-wrapped prose, uniform indent.  This SHOULD
   * still reflow (lines joined) -- that is the feature, not a bug. */
  make_line ("The old library is dim and dusty. Tall shelves lean inward");
  make_line ("overhead, and a single shaft of light falls across the floor");
  make_line ("where a leather-bound book lies open on a reading stand.");
  run ("prose (should reflow into one flowing line)");

  /* Regression: short room title above a long description (Phase 7 case).
   * Title should stay on its own line; description should reflow. */
  make_line ("Library");
  make_line ("The old library is dim and dusty, with tall shelves leaning");
  make_line ("inward overhead and a shaft of light across the floor.");
  run ("title + description (title on own line, body reflows)");

  return 0;
}

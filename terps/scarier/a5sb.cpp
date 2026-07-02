/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- growable output buffer.  See a5sb.h.
 */

#include <stdlib.h>
#include <string.h>

#include "a5sb.h"
#include "a5text.h"   /* A5_CLS_MARK */

void
sb_init (sb_t *b) { b->p = NULL; b->len = b->cap = 0; }

void
sb_puts (sb_t *b, const char *s)
{
  size_t n;
  if (s == NULL) return;
  n = strlen (s);
  if (b->len + n + 1 > b->cap)
    {
      size_t cap = b->cap ? b->cap : 128;
      while (cap < b->len + n + 1) cap *= 2;
      b->p = (char *) realloc (b->p, cap);
      b->cap = cap;
    }
  memcpy (b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = '\0';
}

void
sb_putc_ (sb_t *b, char c) { char t[2] = { c, '\0' }; sb_puts (b, t); }

char *
sb_take (sb_t *b) { return b->p ? b->p : strdup (""); }

void
sb_resolve_cls (sb_t *b, size_t floor)
{
  size_t last = (size_t) -1, i;
  if (b->p == NULL || floor > b->len) return;
  for (i = floor; i < b->len; i++)
    if (b->p[i] == A5_CLS_MARK) last = i;
  if (last == (size_t) -1) return;
  memmove (b->p + floor, b->p + last + 1, b->len - (last + 1) + 1);
  b->len -= (last + 1 - floor);
}

void
sb_pspace (sb_t *b)
{
  /* A trailing <cls> marker is treated like a trailing newline: the join spaces
     would otherwise be stranded before the marker and reappear as spurious
     leading whitespace once finish_turn drops everything up to the marker. */
  if (b->len > 0 && b->p[b->len - 1] != '\n' && b->p[b->len - 1] != A5_CLS_MARK)
    sb_puts (b, "  ");
}

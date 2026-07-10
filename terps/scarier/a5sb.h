/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- growable output buffer (string builder).
 *
 * The turn loop accumulates a turn's rendered text into an sb_t before handing
 * it back to the host.  sb_pspace / sb_resolve_cls implement FrankenDrift's
 * per-commit join-spacing and <cls> wipe semantics (A5_CLS_MARK, a5text.h) that
 * the a5run_* files share.  Split out of a5run.cpp; see a5run.h.
 */

#ifndef SCARIER_A5SB_H
#define SCARIER_A5SB_H

#include <stddef.h>

typedef struct { char *p; size_t len, cap; } sb_t;

void  sb_init  (sb_t *b);
void  sb_puts  (sb_t *b, const char *s);
void  sb_putc_ (sb_t *b, char c);
char *sb_take  (sb_t *b);

/* Resolve any <cls> markers (A5_CLS_MARK) within b->p[floor .. len).  FD's
   Display renders per commit (bCommit=True flushes the accumulated sOutputText
   through one EmitHtml call, where <cls> clears that call's buffer); so a <cls>
   wipes everything back to the LAST commit boundary, not the whole turn.  `floor`
   is the offset of the current commit's start.  Drops everything from `floor` up
   to and including the last marker in the range, leaving the surviving tail. */
void  sb_resolve_cls (sb_t *b, size_t floor);

/* Replace the oldn-byte span at `off` with the NUL-terminated `s` (grows or
   shrinks the buffer).  Out-of-range arguments are ignored. */
void  sb_splice (sb_t *b, size_t off, size_t oldn, const char *s);

/* FrankenDrift's Global.pSpace: before appending the next Display() chunk, the
   accumulator gets two trailing spaces -- UNLESS it is empty or already ends in
   a newline (vbLf).  This is what joins a task's completion message and a
   turn-based event's message onto the same line ("...milk.  The postman...")
   while a message that ends in a newline keeps the next one on a fresh line.
   Scarier formerly forced a '\n' after every message, so multi-message turns
   diverged from FD; call sb_pspace before each message instead. */
void  sb_pspace (sb_t *b);

#endif

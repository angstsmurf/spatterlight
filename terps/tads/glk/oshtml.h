/* oshtml.h -- HTML TADS markup to Glk */

#ifndef OSHTML_H
#define OSHTML_H

#include <stddef.h>

#include "glk.h"

/* The engine's \H+ and \H- (os_start_html, os_end_html) */
void oshtml_start(void);
void oshtml_end(void);

/* Write game text, in the game's character set, to the main window */
void oshtml_print_main(const char *str, size_t len);

/*
 *   Write game text to the status line: store at most 'max' characters of
 *   what it prints in 'out', and return the number stored.  With 'max' 0,
 *   this is the formatter's text for a hidden status line (os_status(2)).
 */
size_t oshtml_print_status(const char *str, size_t len,
                           glui32 *out, size_t max);

/*
 *   Write text of the OS layer's own (the input echo, the MORE prompt) to
 *   the main window as it is, after anything the parser holds back.  Like
 *   line input and clearing the window, it leaves the parser's idea of the
 *   line alone, as the formatter's is left alone: the formatter's first
 *   newline after a command still makes a blank line.
 */
void oshtml_put_plain(const char *str, size_t len);

/*
 *   os_flush in the main window: show the spaces held back at the end of
 *   the line
 */
void oshtml_flush(void);

/* The formatter's text attributes (os_set_text_attr) */
void oshtml_set_os_attr(int attr);

#endif /* OSHTML_H */

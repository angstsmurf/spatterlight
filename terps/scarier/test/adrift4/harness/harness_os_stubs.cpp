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
 * Shared do-nothing OS-layer port for the ADRIFT 4 harnesses that link the
 * interpreter sources directly (badparent, corrupt, precedence, quit, and the
 * scmap dumper).  Each of those used to carry its own byte-identical copy of
 * these thirteen stubs.
 *
 * Deliberately *not* included here is os_read_line(): it is the one hook every
 * harness implements differently -- it is how each one drives the interpreter
 * (a script table, a fixed "quit", a probe on first call).  Each harness still
 * defines its own, and os_read_line_debug() below forwards to whichever one is
 * linked in.
 *
 * This is not sxstubs.cpp: that is the SCARE test-suite's tracing stub layer,
 * and it supplies an os_read_line() of its own, which is exactly what these
 * harnesses need to own.  Harnesses link SRC + their own .cpp + this file.
 */

#include "scarier.h"

void os_print_string (const scr_char *string) { (void) string; }
void os_print_tag (scr_int tag, const scr_char *argument)
{ (void) tag; (void) argument; }
void os_print_string_debug (const scr_char *string) { (void) string; }

void os_play_sound (const scr_char *filepath,
                    scr_int offset, scr_int length, scr_bool is_looping)
{ (void) filepath; (void) offset; (void) length; (void) is_looping; }
void os_stop_sound (void) { }
void os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{ (void) filepath; (void) offset; (void) length; }

void os_display_hints (scr_game game) { (void) game; }

/* Answers yes to everything, including the quit confirmation -- the harnesses
   that reach a confirm at all want it to go through. */
scr_bool os_confirm (scr_int type) { (void) type; return 1; }

void *os_open_file (scr_bool is_save) { (void) is_save; return NULL; }
void os_write_file (void *opaque, const scr_byte *buffer, scr_int length)
{ (void) opaque; (void) buffer; (void) length; }
scr_int os_read_file (void *opaque, scr_byte *buffer, scr_int length)
{ (void) opaque; (void) buffer; (void) length; return 0; }
void os_close_file (void *opaque) { (void) opaque; }

scr_bool os_read_line_debug (scr_char *buffer, scr_int length)
{ return os_read_line (buffer, length); }

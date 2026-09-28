/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#include "scarier.h"

#ifndef SCARIEREXT_PROTOTYPES_H
#define SCARIEREXT_PROTOTYPES_H

/* True and false, unless already defined. */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/*
 * Small utility and wrapper functions.  For printf wrappers, try to apply
 * gcc printf argument checking; this code is cautious about applying the
 * checks.
 */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
extern void sxr_trace (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
extern void sxr_error (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
extern void sxr_fatal (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
#else
extern void sxr_trace (const scr_char *format, ...);
extern void sxr_error (const scr_char *format, ...);
extern void sxr_fatal (const scr_char *format, ...);
#endif
extern void *sxr_malloc (size_t size);
extern void *sxr_realloc (void *pointer, size_t size);
extern void sxr_free (void *pointer);
extern FILE *sxr_fopen (const scr_char *name,
                       const scr_char *extension, const scr_char *mode);
extern scr_char *sxr_trim_string (scr_char *string);
extern scr_char *sxr_normalize_string (scr_char *string);

/* OS stub hooks controller functions. */
extern void stub_attach_handlers (scr_bool (*read_line) (scr_char *, scr_int),
                                  void (*print_string) (const scr_char *),
                                  void *(*open_file) (scr_bool),
                                  scr_int (*read_file)
                                      (void*, scr_byte*, scr_int),
                                  void (*write_file)
                                      (void*, const scr_byte*, scr_int),
                                  void (*close_file) (void*));
extern void stub_detach_handlers (void);
extern void stub_debug_trace (scr_bool flag);

/* Globbing function. */
extern scr_bool glob_match (const scr_char *pattern, const scr_char *string);

#endif

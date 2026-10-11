/*
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
 */

/* The Quest libraries that shipped with Quest itself rather than with the
 * games, and so are never on disk beside one: each is bundled as the text of
 * the library, in a source file of its own that keeps its author's notice
 * (*_builtin.cc).  readfile.cc splices them in where a game !includes them. */

#ifndef QUESTION_BUILTIN_LIBS_HH
#define QUESTION_BUILTIN_LIBS_HH

extern const char question_builtin_standardlib[];   /* standard.lib */
extern const char question_builtin_stdverbs[];      /* stdverbs.lib */
extern const char question_builtin_typelib[];       /* typelib.qlb */
extern const char question_builtin_q3ext[];         /* q3ext.qlb */

#endif /* QUESTION_BUILTIN_LIBS_HH */

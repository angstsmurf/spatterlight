/*
 * Copyright (C) 2006  Mark J. Tilford
 * Copyright (C) 2021-2026  Petter Sjölund
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

#ifndef __general_hh
#define __general_hh

#include <iostream>

typedef unsigned int uint;

/* Debug trace output.  The Question core is littered with cerr traces that fire on
 * the hot path (every object/variable lookup, string eval, command match), so
 * by default QUESTION_DBG is a sink that suppresses the formatting and I/O.  Build
 * with -DQUESTION_DEBUG to route them back to std::cerr.  Use it like cerr:
 *     QUESTION_DBG << "value = " << x << endl;
 */
#ifdef QUESTION_DEBUG
#  define QUESTION_DBG std::cerr
#else
namespace question_detail {
  struct NullStream {
    template <class T> NullStream &operator<< (const T &) { return *this; }
    /* accept manipulators such as std::endl / std::flush */
    NullStream &operator<< (std::ostream & (*) (std::ostream &)) { return *this; }
  };
  inline NullStream &dbg_sink () { static NullStream s; return s; }
}
#  define QUESTION_DBG (question_detail::dbg_sink ())
#endif

#endif

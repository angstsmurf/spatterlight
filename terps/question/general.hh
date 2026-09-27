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

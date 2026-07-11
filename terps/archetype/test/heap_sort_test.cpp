/* heap_sort_test.cpp -- unit regression for the Archetype SORT system object.
 *
 * The three demo games (Gorreven, Starship, Bare) define but never drive the
 * sorter, so the end-to-end walkthroughs give it zero coverage. This test
 * exercises heap_sort.cpp directly (the same drop_str_on_heap / pop_heap path
 * reached at runtime via `INIT SORTER` / `NEXT SORTED`, sys_object.cpp) and
 * asserts the strings come back in ascending order.
 *
 * It guards two bugs that were latent because of that coverage gap:
 *   - pop_heap used 0-based indices against the 1-based access_xarray, an
 *     out-of-bounds vector[-1] read/write (crash / heap corruption); and
 *   - heapdown's sift-down condition was inverted (`!lighter`), mis-ordering
 *     the output once the crash was fixed.
 *
 * It reuses the interpreter objects and CheapGlk, so it is wired as a second
 * glk_main() entry point (see Makefile.headless: regress-heap).
 */

#include "../archetype.h"
#include "../heap_sort.h"

#include <cstdio>

extern "C" {
#include "glk.h"
#include "glkstart.h"
}

using namespace Glk::Archetype;

glkunix_argumentlist_t glkunix_arguments[] = {
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data) {
    (void)data;
    return 1;
}

// Write straight to stdout: this is a unit test, not a game, and no Glk window
// is opened, so CheapGlk would otherwise drop glk_put_string output.
static void put(const char *s) { std::fputs(s, stdout); }

void glk_main(void) {
    new Archetype();  // sets g_vm; the constructor makes no Glk calls
    heap_sort_init();

    // Deliberately unsorted, including a would-be-root that must sift down.
    const char *input[] = { "pear", "apple", "cherry", "banana", "date" };
    const char *expected[] = { "apple", "banana", "cherry", "date", "pear" };
    const int N = 5;

    for (int i = 0; i < N; ++i)
        drop_str_on_heap(String(input[i]));

    bool ok = true;
    int n = 0;
    void *p;
    while (pop_heap(p)) {
        String cur = *static_cast<StringPtr>(p);
        put("  pop: ");
        put(cur.c_str());
        put("\n");
        if (n >= N || cur != String(expected[n]))
            ok = false;
        ++n;
    }
    if (n != N)
        ok = false;

    put(ok ? "HEAP SORT TEST: PASS\n" : "HEAP SORT TEST: FAIL\n");
}

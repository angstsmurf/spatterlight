/* glk_main.cpp -- C++ Glk entry point.
 *
 * Spatterlight calls glk_main() after the C startup code in glkstart.c has
 * captured the filename. We open the game file, instantiate the Archetype
 * engine and run it.
 */

#include "archetype.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "glk.h"
extern const char *archetype_storyfile;
}

void glk_main(void) {
    if (!archetype_storyfile) {
        glk_put_string((char *)"Archetype: no game file specified.\n");
        return;
    }

    Glk::Archetype::Archetype *vm = new Glk::Archetype::Archetype();

    if (!vm->_gameFile.open(archetype_storyfile)) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf),
            "Archetype: could not open '%s'.\n", archetype_storyfile);
        glk_put_string(buf);
        delete vm;
        return;
    }

    vm->runGame();
    delete vm;
}

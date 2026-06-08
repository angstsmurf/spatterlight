/* glk_main.cpp -- C++ Glk entry point for the Comprehend port. */

#include "comprehend.h"
#include "apple2.h"
#include "game_data.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>

extern "C" {
#include "glk.h"
extern const char *comprehend_storyfile;
extern const char *comprehend_gameid;
}

// Guess the game id from the filename when -g wasn't supplied. This is a
// fallback for direct double-launches; Spatterlight normally passes -g.
static Common::String guessGameId(const char *path) {
    Common::String p(path ? path : "");
    Common::String lower = p;
    lower.toLowercase();
    if (lower.contains("cc1") || lower.contains("crimson"))     return Common::String("crimsoncrown");
    if (lower.contains("ootopos") || lower.contains("oo-topos")) return Common::String("ootopos");
    if (lower.contains("talisman"))                              return Common::String("talisman");
    if (lower.contains("trv2") || lower.contains("transv2") || lower.contains("transylvaniav2"))
        return Common::String("transylvaniav2");
    return Common::String("transylvania");
}

void glk_main(void) {
    if (!comprehend_storyfile) {
        glk_put_string((char *)"Comprehend: no game file specified.\n");
        return;
    }

    // Comprehend's game classes open their data file by basename
    // (tr.gda / cc1.gda / g0), so we cd into the directory of the
    // storyfile first. This matches how ScummVM's SearchMan picks up
    // every file in the game's directory.
    Common::String path(comprehend_storyfile);
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos)
        chdir(Common::String(path.c_str(), slash).c_str());

    // If the storyfile is an Apple II disk image, extract its files (and any
    // companion disk sides) into the in-memory VFS so the engine can open
    // them by name, just as it would the DOS/PC files on disk.
    bool fromDisk = false;
    if (Glk::Comprehend::isAppleDiskImage(comprehend_storyfile)) {
        if (Glk::Comprehend::loadAppleDiskImage(comprehend_storyfile) > 0)
            fromDisk = true;
        else
            glk_put_string((char *)"Comprehend: could not read Apple II disk image.\n");
    }

    Glk::Comprehend::Comprehend *vm = new Glk::Comprehend::Comprehend();

    Common::String gid;
    if (comprehend_gameid && *comprehend_gameid) gid = comprehend_gameid;
    else if (fromDisk)                           gid = Glk::Comprehend::guessAppleGameId();
    if (gid.empty())                             gid = guessGameId(comprehend_storyfile);
    vm->setGameId(gid);
    vm->setGameFile(comprehend_storyfile);

    vm->runGame();
    delete vm;
}

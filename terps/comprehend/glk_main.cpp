/* glk_main.cpp -- C++ Glk entry point for the Comprehend port. */

#include "comprehend.h"
#include "apple2.h"
#include "game_data.h"
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

extern "C" {
#include "glk.h"
extern const char *comprehend_storyfile;
extern const char *comprehend_gameid;
}

// Guess the game id from the filename when -g wasn't supplied, for direct
// double-launches.
static Common::String guessGameId(const char *path) {
	static const struct {
		const char *substring;
		const char *gameId;
	} kFileNames[] = {
		{ "cc1",            "crimsoncrown" },
		{ "crimson",        "crimsoncrown" },
		{ "ootopos",        "ootopos" },
		{ "oo-topos",       "ootopos" },
		{ "talisman",       "talisman" },
		{ "mirror",         "covetedmirror" },
		{ "coveted",        "covetedmirror" },
		{ "trv2",           "transylvaniav2" },
		{ "transv2",        "transylvaniav2" },
		{ "transylvaniav2", "transylvaniav2" },
	};

	Common::String lower(path ? path : "");
	lower.toLowercase();
	for (const auto &name : kFileNames) {
		if (lower.contains(name.substring))
			return Common::String(name.gameId);
	}
	return Common::String("transylvania");
}

void glk_main(void) {
	if (!comprehend_storyfile) {
		glk_put_string((char *)"Comprehend: no game file specified.\n");
		return;
	}

	// Extract an Apple II disk image and its companion sides into the in-memory
	// filesystem. Must run before the chdir below, since a relative storyfile
	// path would then fail to open.
	bool fromDisk = false;
	if (Glk::Comprehend::isAppleDiskImage(comprehend_storyfile)) {
		if (Glk::Comprehend::loadAppleDiskImage(comprehend_storyfile) > 0)
			fromDisk = true;
		else
			glk_put_string((char *)"Comprehend: could not read Apple II disk image.\n");
	}

	// The game classes open their data files by basename (tr.gda, cc1.gda, g0).
	Common::String path(comprehend_storyfile);
	size_t slash = path.find_last_of("/\\");
	if (slash != std::string::npos)
		chdir(Common::String(path.c_str(), slash).c_str());

	Glk::Comprehend::Comprehend *vm = new Glk::Comprehend::Comprehend();

	Common::String gid;
	if (comprehend_gameid && *comprehend_gameid) gid = comprehend_gameid;
	else if (fromDisk)                           gid = Glk::Comprehend::guessAppleGameId();
	else                                         gid = Glk::Comprehend::guessDosGameId();
	if (gid.empty())                             gid = guessGameId(comprehend_storyfile);
	vm->setGameId(gid);
	vm->setGameFile(comprehend_storyfile);

	vm->runGame();
	delete vm;
}

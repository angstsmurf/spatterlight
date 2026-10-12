/* game_traits.h -- what the shared loaders and renderers need to know about
 * each game, in one table (game_traits.cpp), instead of comparing the game id
 * string at every place a release differs.
 */

#ifndef GLK_COMPREHEND_GAME_TRAITS_H
#define GLK_COMPREHEND_GAME_TRAITS_H

#include <cstdint>

namespace Glk {
namespace Comprehend {

class ComprehendGame;

struct GameTraits {
	const char *id;
	ComprehendGame *(*create)();

	// --- Game data layout (game_data.cpp) ---

	// All strings live in the separate string files: the game file has no
	// string table of its own, and its addr_strings_end header field points
	// straight at the replace-word table (Talisman).
	bool stringsOnlyInFiles;
	// Stop reading the function table after this many entries; 0 = read to the
	// table's own terminator (Talisman's runs on into other data).
	uint16_t functionTableLimit;
	// OO-Topos pads the first and fifth string bank with a leading empty
	// string so its string indices line up.
	bool padsStringBanks;
	// Apple II disk only: the number of empty strings to put ahead of the
	// string files, which are that many entries short at the front compared
	// with the DOS data the string indices were derived from.
	uint8_t appleStringLead;
	// Apple II disk only: the string banks have no 4-byte header; the offset
	// index starts at byte 0 (The Coveted Mirror).
	bool appleStringBanksHeaderless;

	// --- Pictures ---

	// Apple II: the older Graphics Magician dialect, which never emits op15,
	// ends images on op7 and needs the full-screen fill clip.
	bool gmLegacyFormat;
	// Apple II: where the title picture starts in the T0 file, past the copy
	// protection and loader stub.
	uint16_t appleTitleOffset;
	// Apple II: the title picture is drawn on a black screen, not a white one.
	bool appleTitleOnBlack;
	// Apple II: the persistent logo and hourglass panel to the right of every
	// picture (The Coveted Mirror).
	bool hasHourglassPanel;

};

// The traits for a game id, or nullptr if the id is not a known game.
const GameTraits *findGameTraits(const char *id);

} // namespace Comprehend
} // namespace Glk

#endif

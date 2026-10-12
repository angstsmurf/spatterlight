/* game_traits.cpp -- the per-game table. See game_traits.h. */

#include "game_traits.h"
#include "game_cc.h"
#include "game_cm.h"
#include "game_oo.h"
#include "game_tm.h"
#include "game_tr1.h"
#include "game_tr2.h"
#include <cstring>

namespace Glk {
namespace Comprehend {

template <typename T>
static ComprehendGame *make() { return new T(); }

static const GameTraits kGames[] = {
	{
		"transylvania", make<TransylvaniaGame1>,
		/* stringsOnlyInFiles */ false, /* functionTableLimit */ 0,
		/* padsStringBanks */ false, /* appleStringLead */ 2,
		/* appleStringBanksHeaderless */ false,
		/* gmLegacyFormat */ true, /* appleTitleOffset */ 0xFE,
		/* appleTitleOnBlack */ false, /* hasHourglassPanel */ false
	},
	{
		"transylvaniav2", make<TransylvaniaGame2>,
		false, 0, false, 0, false,
		false, 0, false, false
	},
	{
		"crimsoncrown", make<CrimsonCrownGame>,
		false, 0, false, 0, false,
		true, 0x100, false, false
	},
	{
		"ootopos", make<OOToposGame>,
		false, 0, true, 1, false,
		false, 0x100, true, false
	},
	{
		// Talisman's T0 is a plain Graphics Magician image from offset 0.
		"talisman", make<TalismanGame>,
		true, 0x1d8, false, 0, false,
		false, 0, false, false
	},
	{
		"covetedmirror", make<CovetedMirrorGame>,
		false, 0, false, 0, true,
		false, 0x100, false, true
	}
};

const GameTraits *findGameTraits(const char *id) {
	for (const GameTraits &t : kGames)
		if (!strcmp(t.id, id))
			return &t;
	return nullptr;
}

} // namespace Comprehend
} // namespace Glk

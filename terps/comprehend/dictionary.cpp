/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "comprehend.h"
#include "game.h"
#include "game_data.h"
#include "dictionary.h"

namespace Glk {
namespace Comprehend {

static bool word_match(Word *word, const char *string) {
	/* _len is the pre-computed strlen(word->_word), cached at load time so this
	 * hot path (scanned linearly for every input word) avoids re-strlen-ing the
	 * dictionary word two or three times per comparison. */
	uint8 wlen = word->_len;

	/* Words less than 6 characters must match exactly */
	if (wlen < 6 && strlen(string) != wlen)
		return false;

	/* Dictionary words are stored lower-case; match the player's input
	 * case-insensitively so e.g. "BOW" and "bow" both parse. */
	return strncasecmp(word->_word, string, wlen) == 0;
}

Word *dict_find_word_by_string(ComprehendGame *game,
							   const char *string) {
	uint i;

	if (!string)
		return nullptr;

	for (i = 0; i < game->_words.size(); i++)
		if (word_match(&game->_words[i], string))
			return &game->_words[i];

	return nullptr;
}

Word *dict_find_word_by_index_type(ComprehendGame *game,
								   uint8 index, uint8 type) {
	uint i;

	for (i = 0; i < game->_words.size(); i++) {
		if (game->_words[i]._index == index &&
		        game->_words[i]._type == type)
			return &game->_words[i];
	}

	return nullptr;
}

Word *find_dict_word_by_index(ComprehendGame *game,
							  uint8 index, uint8 type_mask) {
	uint i;

	for (i = 0; i < game->_words.size(); i++) {
		if (game->_words[i]._index == index &&
		        (game->_words[i]._type & type_mask) != 0)
			return &game->_words[i];
	}

	return nullptr;
}

bool dict_match_index_type(ComprehendGame *game, const char *word,
						   uint8 index, uint8 type_mask) {
	uint i;

	for (i = 0; i < game->_words.size(); i++)
		if (game->_words[i]._index == index &&
		        ((game->_words[i]._type & type_mask) != 0) &&
		        word_match(&game->_words[i], word))
			return true;

	return false;
}

} // namespace Comprehend
} // namespace Glk

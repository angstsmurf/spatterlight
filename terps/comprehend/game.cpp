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

#include "game.h"
#include "comprehend_compat.h"
#include "comprehend.h"

#include "dictionary.h"
#include "draw_surface.h"
#include "game_data.h"
#include "pics.h"
#include "graphics_magician_dhgr.h"
#include "graphics_magician_pcjr.h"

namespace Glk {
namespace Comprehend {

void Sentence::clear() {
	for (uint idx = 0; idx < 4; ++idx)
		_words[idx].clear();
	for (uint idx = 0; idx < 6; ++idx)
		_formattedWords[idx] = 0;

	_nr_words = 0;
	_specialOpcodeVal2 = 0;
	_wordFlags = 0;
}

void Sentence::copyFrom(const Sentence &src, bool copyNoun) {
	for (uint idx = (copyNoun ? 0 : 1); idx < 6; ++idx)
		_formattedWords[idx] = src._formattedWords[idx];
}

void Sentence::format() {
	for (uint idx = 0; idx < 6; ++idx)
		_formattedWords[idx] = 0;
	_wordFlags = 0;
	byte wordTypes[5] = { 0, 0, 0, 0, 0 };

	for (uint idx = 0; idx < _nr_words; ++idx) {
		const Word &w = _words[idx];

		if (w._type & 8) {
			if (w._type < 24) {
				int index, type;

				if (w._type & 0xf0 & wordTypes[2]) {
					index = _formattedWords[2];
					type = wordTypes[2];
				} else if (w._type & 0xf0 & wordTypes[3]) {
					index = _formattedWords[3];
					type = wordTypes[3];
				} else {
					continue;
				}

				if (!_formattedWords[2]) {
					_formattedWords[2] = index;
					wordTypes[2] = type;
				} else if (!_formattedWords[3]) {
					_formattedWords[3] = index;
					wordTypes[3] = type;
				}
			} else {
				if (w._type == 8)
					_specialOpcodeVal2 = 1;
				else if (w._type == 9)
					_specialOpcodeVal2 = 2;
			}
		} else {
			int val = w._type & 0xf0;

			if (val) {
				if ((w._type & 1) && !_formattedWords[0]) {
					_formattedWords[0] = w._index;
				} else if (!_formattedWords[2]) {
					_formattedWords[2] = w._index;
					wordTypes[2] = val;
				} else if (!_formattedWords[3]) {
					_formattedWords[3] = w._index;
					wordTypes[3] = val;
				}
			} else if (w._type & 1) {
				if (!_formattedWords[0]) {
					_formattedWords[0] = w._index;
				} else if (!_formattedWords[1]) {
					_formattedWords[1] = w._index;
				}
			} else if (w._type == 2) {
				if (!_formattedWords[4])
					_formattedWords[4] = w._index;
			} else if (w._type == 4) {
				if (!_formattedWords[5])
					_formattedWords[5] = w._index;
			}
		}
	}

	// The first noun slot's gender/number nibble drives article agreement for
	// the @-replacement words (OPCODE_SET_STRING_REPLACEMENT3).  Mirrors the
	// Apple II cm_format_sentence_words setting cm_current_noun_flags ($5b6a).
	_wordFlags = wordTypes[2];
}

/*-------------------------------------------------------*/

ComprehendGame::ComprehendGame() : _gameStrings(nullptr), _ended(false),
		_functionNum(0), _specialOpcode(0), _nounState(NOUNSTATE_INITIAL),
		_inputLineIndex(0), _currentRoomCopy(-1), _roomBeforeTurn(-1), _redoLine(REDO_NONE) {
	Common::fill(&_inputLine[0], &_inputLine[INPUT_LINE_SIZE], 0);
}

ComprehendGame::~ComprehendGame() {
}

void ComprehendGame::synchronizeSave(Common::Serializer &s) {
	uint dir, i;
	size_t nr_rooms, nr_items;

	s.syncAsUint16LE(_currentRoom);

	// Variables
	for (i = 0; i < ARRAY_SIZE(_variables); i++)
		s.syncAsUint16LE(_variables[i]);

	// Flags
	for (i = 0; i < ARRAY_SIZE(_flags); i++)
		s.syncAsByte(_flags[i]);

	// Rooms. Note that index 0 is the player's inventory
	nr_rooms = _rooms.size();
	s.syncAsByte(nr_rooms);
	assert(nr_rooms == _rooms.size());

	for (i = 1; i < _rooms.size(); ++i) {
		s.syncAsUint16LE(_rooms[i]._stringDesc);
		for (dir = 0; dir < NR_DIRECTIONS; dir++)
			s.syncAsByte(_rooms[i]._direction[dir]);

		s.syncAsByte(_rooms[i]._flags);
		s.syncAsByte(_rooms[i]._graphic);
	}

	// Objects
	nr_items = _items.size();
	s.syncAsByte(nr_items);
	assert(nr_items == _items.size());

	for (i = 0; i < _items.size(); ++i)
		_items[i].synchronize(s);

	_redoLine = REDO_NONE;
}

Common::String ComprehendGame::stringLookup(uint16 index) {
	uint16 string;
	uint8 table;

	/*
	 * There are two tables of strings. The first is stored in the main
	 * game data file, and the second is stored in multiple string files.
	 *
	 * In instructions string indexes are split into a table and index
	 * value. In other places such as the save files strings from the
	 * main table are occasionally just a straight 16-bit index. We
	 * convert all string indexes to the former case so that we can handle
	 * them the same everywhere.
	 */
	table = (index >> 8) & 0xff;
	string = index & 0xff;

	switch (table) {
	case 0x81:
	case 0x01:
		string += 0x100;
	/* Fall-through */
	case 0x00:
	case 0x80:
		if (string < _strings.size())
			return _strings[string];
		break;

	case 0x04:
	case 0x84:
		// The Coveted Mirror has twelve extra-string banks (MA-ML, up to 768
		// strings), so it indexes a third sub-table the other games never use.
		string += 0x100;
	/* Fall-through */
	case 0x03:
	case 0x83:
		string += 0x100;
	/* Fall-through */
	case 0x02:
	case 0x82:
		// Item long/short descriptions store the table byte without the high
		// bit set (0x02/0x03/0x04), the same way instructions use 0x82/0x83/0x84.
		if (string < _strings2.size())
			return _strings2[string];
		break;
	}

	return Common::String::format("BAD_STRING(%.4x)", index);
}

Common::String ComprehendGame::instrStringLookup(uint8 index, uint8 table) {
	return stringLookup(table << 8 | index);
}

int ComprehendGame::console_get_key() {
	return g_comprehend->readChar();
}

void ComprehendGame::console_println(const char *text) {
	const char *replace, *word = nullptr, *p = text;
	char bad_word[64];
	int word_len = 0;

	if (!text) {
		g_comprehend->print("\n");
		return;
	}

	while (*p) {
		switch (*p) {
		case '\n':
			word = nullptr;
			word_len = 0;
			g_comprehend->print("\n");
			p++;
			break;

		case '@':
			/* Replace word */
			if (_replaceWordIsNumber) {
				/*
				 * Number mode: render a variable's value as a decimal
				 * number (e.g. Talisman's bazaar prices). Mirrors the
				 * original interpreter's capital-space number branch,
				 * which formats variables[index] as decimal.
				 */
				snprintf(bad_word, sizeof(bad_word), "%u",
				         _variables[_replaceNumberVar]);
				word = bad_word;
			} else if (_currentReplaceWord >= _replaceWords.size()) {
				snprintf(bad_word, sizeof(bad_word),
				         "[BAD_REPLACE_WORD(%.2x)]",
				         _currentReplaceWord);
				word = bad_word;
			} else {
				word = _replaceWords[_currentReplaceWord].c_str();
			}
			word_len = strlen(word);
			p++;
			break;

		default:
			/* Find next space */
			word_len = strcspn(p, " \n");
			if (word_len == 0) {
				/*
				 * A space at the start of a parse iteration -- e.g. the
				 * indentation that follows a newline in a multi-line string
				 * such as Talisman's death menu. ('\n' has its own case, so
				 * strcspn can only be zero here for a space.) Emit one space
				 * and advance past the run; otherwise p never moves and we
				 * spin forever.
				 */
				g_comprehend->print(" ");
				do {
					p++;
				} while (*p == ' ');
				break;
			}

			/*
			 * If this word contains a replacement symbol, then
			 * print everything before the symbol.
			 */
			replace = strchr(p, '@');
			if (replace)
				word_len = replace - p;

			word = p;
			p += word_len;
			break;
		}

		if (!word || !word_len)
			continue;

		Common::String wordStr(word, word_len);
		g_comprehend->print("%s", wordStr.c_str());

		if (*p == ' ') {
			g_comprehend->print(" ");
			p++;

			/* Skip any double spaces */
			while (*p == ' ')
				p++;
		}
	}

	g_comprehend->print("\n");
}

Room *ComprehendGame::get_room(uint16 index) {
	/* Room zero is reserved for the players inventory */
	if (index == 0)
		error("Room index 0 (player inventory) is invalid");

	if (index >= (int)_rooms.size())
		error("Room index %d is invalid", index);

	return &_rooms[index];
}

Item *ComprehendGame::get_item(uint16 index) {
	if (index >= _items.size())
		error("Bad item %d\n", index);

	return &_items[index];
}

// Match a "1, 2, or 3"-style numbered slot list starting at s[i], tolerating
// any spacing/punctuation between the digits ("1, 2, or 3", "1,2,or 3", ...).
// Returns the length of the matched run, or 0 if s[i] doesn't begin one. A run
// must ascend strictly 1 -> 2 -> 3 with only separators (spaces, commas, the
// words "or"/"and") in between.
static size_t matchSlotList(const Common::String &s, size_t i) {
	if (i >= s.size() || s[i] != '1')
		return 0;

	int last = 1;
	size_t j = i + 1;
	while (j < s.size()) {
		char c = s[j];
		if (c >= '1' && c <= '3') {
			if (c - '0' != last + 1)
				return 0;                 // not a strict 1->2->3 ascent
			last = c - '0';
			j++;
			if (last == 3)
				return j - i;             // matched the whole list
		} else if (c == ',' || c == ' ') {
			j++;
		} else if ((c == 'o' || c == 'O') && j + 1 < s.size() &&
		           (s[j + 1] == 'r' || s[j + 1] == 'R')) {
			j += 2;
		} else if ((c == 'a' || c == 'A') && j + 2 < s.size() &&
		           s[j + 1] == 'n' && s[j + 2] == 'd') {
			j += 3;
		} else {
			return 0;                     // stray text: not a slot list
		}
	}
	return 0;
}

// The original games' SAVE/RESTORE prompts instruct the player to pick a
// numbered slot, but we now route to Spatterlight's file dialog. Keep the
// games' flavour text but rewrite the slot reference so it reads naturally.
// The phrasing differs per game, e.g.
//   "Emergency Saving Procedure:\nChoose 1, 2, or 3"  (Oo-Topos)
//   "...Ghost of Transylvania Future. Save 1,2,or 3?" (Transylvania)
//   "Which game do you want to save (1-3)?"           (Crimson Crown)
//   "...whispering \"Saveth which game? (1-8)\""      (Talisman)
static Common::String rewriteSlotPrompt(Common::String s) {
	// "Choose 1, 2, or 3" -> "Choose a game" (any spacing of the list).
	for (size_t i = 0; i < s.size(); i++) {
		size_t len = matchSlotList(s, i);
		if (len) {
			s.replace(i, len, "a game");
			i += 5;  // length of "a game" - 1
		}
	}

	// Strip any "(1-N)" parenthetical (e.g. "(1-3)", "(1-8)"), along with a
	// single space in front of it, so "...save (1-3)?" -> "...save?".
	size_t open;
	while ((open = s.find("(1-")) != std::string::npos) {
		size_t close = s.find(')', open);
		if (close == std::string::npos)
			break;
		size_t start = (open > 0 && s[open - 1] == ' ') ? open - 1 : open;
		s.erase(start, close - start + 1);
	}

	return s;
}

void ComprehendGame::game_save() {
	// Show the game's own save message (slot reference rewritten), then route
	// to the standard Spatterlight save-file prompt rather than the original
	// numbered-slot menu.
	console_println(rewriteSlotPrompt(_strings[STRING_SAVE_GAME]).c_str());
	(void)g_comprehend->saveGamePrompt();
}

bool ComprehendGame::game_restore() {
	console_println(rewriteSlotPrompt(_strings[STRING_RESTORE_GAME]).c_str());
	return g_comprehend->loadGamePrompt().getCode() == Common::kNoError;
}

void ComprehendGame::restartGame() {
	// Reload the game from the start, bypassing the in-game restart prompt.
	// Subclasses override to redo any extra reload work (e.g. Talisman's
	// strings, Crimson Crown's disk reset).
	_ended = false;
	loadGame();
	g_comprehend->clearUndo();  // can't undo across a restart
	_updateFlags = UPDATE_ALL;
}

bool ComprehendGame::handle_restart() {
	console_println(stringLookup(_gameStrings->game_restart).c_str());
	_ended = false;

	if (tolower(console_get_key()) == 'r') {
		loadGame();
		g_comprehend->clearUndo();  // can't undo across a restart
		_updateFlags = UPDATE_ALL;
		return true;
	} else {
		g_comprehend->quitGame();
		return false;
	}
}

Item *ComprehendGame::get_item_by_noun(byte noun) {
	uint i;

	if (!noun)
		return nullptr;

	/*
	 * FIXME - in oo-topos the word 'box' matches more than one object
	 *         (the box and the snarl-in-a-box). The player is unable
	 *         to drop the latter because this will match the former.
	 */
	for (i = 0; i < _items.size(); i++)
		if (_items[i]._word == noun)
			return &_items[i];

	return nullptr;
}

int ComprehendGame::get_item_id(byte noun) {
	for (int i = 0; i < (int)_items.size(); i++)
		if (_items[i]._word == noun)
			return i;

	return -1;
}

void ComprehendGame::update_graphics() {
	Item *item;
	Room *room;
	int type;
	uint i;

	if (!g_comprehend->isGraphicsEnabled())
		return;

	type = roomIsSpecial(_currentRoomCopy, nullptr);

	// A full repaint (UPDATE_GRAPHICS) clears the background and redraws the
	// location plus its items; an items-only refresh (UPDATE_GRAPHICS_ITEMS,
	// e.g. an object moved into the room) just layers items on the existing
	// screen. Either way we hand the list of pictures to the host, which knows
	// what is already on screen and paints unchanged repaints instantly instead
	// of re-running the slow-draw reveal.
	bool fullRepaint = (_updateFlags & UPDATE_GRAPHICS) != 0;

	switch (type) {
	case ROOM_IS_DARK:
		if (fullRepaint)
			g_comprehend->clearScreen(false);
		break;

	case ROOM_IS_TOO_BRIGHT:
		if (fullRepaint)
			g_comprehend->clearScreen(true);
		break;

	default:
		if (fullRepaint ||
		        (_updateFlags & UPDATE_GRAPHICS_ITEMS)) {
			Common::Array<uint> pics;
			if (fullRepaint) {
				room = get_room(_currentRoom);
				pics.push_back((uint)(room->_graphic - 1) + LOCATIONS_OFFSET);
			}
			for (i = 0; i < _items.size(); i++) {
				item = &_items[i];

				// Skip items whose picture the bytecode has suppressed via
				// flag bit 0x40 (ITEMF_NO_PICTURE). The original interpreter's
				// per-turn presentation draws an item's picture only when this
				// bit is clear (cm_describe_room_and_items $4dc1: object picture
				// drawn iff (item_flags & 0x40) == 0); there is no dark-room
				// test in the draw path. This is a *separate* flag from the
				// 0x80 ITEMF_INVISIBLE the text listing uses: e.g. in The
				// Coveted Mirror the medallion in the unlit room behind the
				// prison tower's hole has 0x40 set (not drawn over the black
				// dark-room screen), while the moved-bed item has only 0x80 set
				// -- hidden from the text listing but its picture (the shifted
				// bed and the hole behind it) must still be drawn. Gating on
				// 0x80 wrongly hid the bed picture.
				if (item->_room == _currentRoom &&
				        item->_graphic != 0 &&
				        !(item->_flags & ITEMF_NO_PICTURE))
					pics.push_back((uint)(item->_graphic - 1) + ITEMS_OFFSET);
			}
			if (fullRepaint)
				g_comprehend->paintBackground(pics);
			else
				g_comprehend->paintOverlay(pics);
		}
		break;
	}
}

void ComprehendGame::describe_objects_in_current_room() {
	Item *item;
	size_t count = 0;
	uint i;

	for (i = 0; i < _items.size(); i++) {
		item = &_items[i];

		if (item->_room == _currentRoom && item->_stringDesc != 0
				&& !(item->_flags & ITEMF_INVISIBLE))
			count++;
	}

	if (count > 0) {
		console_println(stringLookup(STRING_YOU_SEE).c_str());

		for (i = 0; i < _items.size(); i++) {
			item = &_items[i];

			if (item->_room == _currentRoom && item->_stringDesc != 0
					&& !(item->_flags & ITEMF_INVISIBLE))
				console_println(stringLookup(item->_stringDesc).c_str());
		}
	}
}

void ComprehendGame::updateRoomDesc() {
	Room *room = get_room(_currentRoom);
	uint room_desc_string = room->_stringDesc;
	roomIsSpecial(_currentRoom, &room_desc_string);

	Common::String desc = stringLookup(room_desc_string);
	g_comprehend->printRoomDesc(desc);
}

void ComprehendGame::transcribeCurrentRoom() {
	// Log the current room description to the transcript (used when a transcript
	// is turned on mid-game, so it captures the room the player is standing in).
	Room *room = get_room(_currentRoom);
	uint room_desc_string = room->_stringDesc;
	roomIsSpecial(_currentRoom, &room_desc_string);

	Common::String desc = stringLookup(room_desc_string);
	g_comprehend->redirectOutputToTranscript(true);
	console_println(desc.c_str());
	g_comprehend->redirectOutputToTranscript(false);
}

void ComprehendGame::update() {
	Room *room = get_room(_currentRoom);
	uint room_type, room_desc_string;

	update_graphics();

	/* Check if the room is special (dark, too bright, etc) */
	room_desc_string = room->_stringDesc;
	room_type = roomIsSpecial(_currentRoom, &room_desc_string);

	if (_updateFlags & UPDATE_ROOM_DESC) {
		Common::String desc = stringLookup(room_desc_string);
		// When the room description is shown in the status grid (printRoomDesc),
		// don't duplicate it into the scroll-back buffer -- but still record it
		// in the transcript, which has no status grid. If there is no grid, fall
		// back to printing it in the buffer as before.
		if (g_comprehend->_statusWindow) {
			g_comprehend->redirectOutputToTranscript(true);
			console_println(desc.c_str());
			g_comprehend->redirectOutputToTranscript(false);
		} else {
			console_println(desc.c_str());
		}
		g_comprehend->printRoomDesc(desc.c_str());
	}

	if ((_updateFlags & UPDATE_ITEM_LIST) && room_type == ROOM_IS_NORMAL)
		describe_objects_in_current_room();

	_updateFlags = 0;
}

void ComprehendGame::move_to(uint8 room) {
	if (room >= (int)_rooms.size())
		error("Attempted to move to invalid room %.2x\n", room);

	_currentRoom = _currentRoomCopy = room;
	_updateFlags = (UPDATE_GRAPHICS | UPDATE_ROOM_DESC |
	                      UPDATE_ITEM_LIST);
}

size_t ComprehendGame::num_objects_in_room(int room) {
	size_t count = 0, i;

	for (i = 0; i < _items.size(); i++)
		if (_items[i]._room == room)
			count++;

	return count;
}

void ComprehendGame::move_object(Item *item, int new_room) {
	unsigned obj_weight = item->_flags & ITEMF_WEIGHT_MASK;

	if (item->_room == new_room)
		return;

	// On V1 interpreters variable 0 is the persisted inventory weight; keep it
	// in sync here. V2 interpreters instead reserve variable 0 as the input-
	// number register (see hasNumberRegister) and recompute inventory weight on
	// demand via weighInventory(), so writing weight here would corrupt the
	// number register.
	if (!hasNumberRegister()) {
		if (item->_room == ROOM_INVENTORY) {
			/* Removed from player's inventory */
			_variables[VAR_INVENTORY_WEIGHT] -= obj_weight;
		}
		if (new_room == ROOM_INVENTORY) {
			/* Moving to the player's inventory */
			_variables[VAR_INVENTORY_WEIGHT] += obj_weight;
		}
	}

	if (item->_room == _currentRoom) {
		/* Item moved away from the current room */
		_updateFlags |= UPDATE_GRAPHICS;

	} else if (new_room == _currentRoom) {
		/*
		 * Item moved into the current room. Only the item needs a
		 * redraw, not the whole room.
		 */
		_updateFlags |= (UPDATE_GRAPHICS_ITEMS |
		                       UPDATE_ITEM_LIST);
	}

	item->_room = new_room;
}

void ComprehendGame::eval_instruction(FunctionState *func_state,
		const Function &func, uint functionOffset, const Sentence *sentence) {

	const Instruction *instr = &func[functionOffset];

	if (DebugMan.isDebugChannelEnabled(kDebugScripts)) {
		Common::String line;
		if (!instr->_isCommand) {
			line += "? ";
		} else {
			if (func_state->_testResult)
				line += "+ ";
			else
				line += "- ";
		}

		line += Common::String::format("%.2x  ", functionOffset);
		line += g_debugger->dumpInstruction(this, func_state, instr);
		debugC(kDebugScripts, "%s", line.c_str());
	}

	if (func_state->_orCount)
		func_state->_orCount--;

	if (instr->_isCommand) {
		bool do_command;

		func_state->_inCommand = true;
		do_command = func_state->_testResult;

		if (func_state->_orCount != 0)
			g_comprehend->print("Warning: or_count == %d\n",
			                    func_state->_orCount);
		func_state->_orCount = 0;

		if (!do_command)
			return;

		func_state->_elseResult = false;
		func_state->_executed = true;

	} else {
		if (func_state->_inCommand) {
			/* Finished command sequence - clear test result */
			func_state->_inCommand = false;
			func_state->_testResult = false;
			func_state->_and = false;
		}
	}

	execute_opcode(instr, sentence, func_state);
}

void ComprehendGame::eval_function(uint functionNum, const Sentence *sentence) {
	FunctionState func_state;
	uint i;

	const Function &func = _functions[functionNum];
	func_state._elseResult = true;
	func_state._executed = false;

	debugC(kDebugScripts, "Start of function %.4x", functionNum);

	for (i = 0; i < func.size(); i++) {
		if (func_state._executed && !func[i]._isCommand) {
			/*
			 * At least one command has been executed and the
			 * current instruction is a test. Exit the function.
			 */
			break;
		}

		eval_instruction(&func_state, func, i, sentence);
	}

	debugC(kDebugScripts, "End of function %.4x\n", functionNum);
}

void ComprehendGame::skip_whitespace(const char **p) {
	while (**p && Common::isSpace(**p))
		(*p)++;
}

void ComprehendGame::skip_non_whitespace(const char **p) {
	while (**p && !Common::isSpace(**p) && **p != ',' && **p != '\n')
		(*p)++;
}

bool ComprehendGame::handle_sentence(Sentence *sentence) {
	if (sentence->_nr_words == 1 && !strcmp(sentence->_words[0]._word, "quit")) {
		g_comprehend->quitGame();
		return true;
	}

	// Set up default sentence
	Common::Array<byte> words;
	const byte *src = &sentence->_formattedWords[0];

	if (src[1] && src[3]) {
		words.clear();

		for (int idx = 0; idx < 4; ++idx)
			words.push_back(src[idx]);

		if (handle_sentence(0, sentence, words))
			return true;
	}

	if (src[1]) {
		words.clear();

		for (int idx = 0; idx < 3; ++idx)
			words.push_back(src[idx]);

		if (handle_sentence(1, sentence, words))
			return true;
	}

	if (src[3] && src[4]) {
		words.clear();

		words.push_back(src[4]);
		words.push_back(src[0]);
		words.push_back(src[2]);
		words.push_back(src[3]);

		if (handle_sentence(2, sentence, words))
			return true;
	}

	if (src[4]) {
		words.clear();

		words.push_back(src[4]);
		words.push_back(src[0]);
		words.push_back(src[2]);

		if (handle_sentence(3, sentence, words))
			return true;
	}

	if (src[3]) {
		words.clear();

		words.push_back(src[0]);
		words.push_back(src[2]);
		words.push_back(src[3]);

 		if (handle_sentence(4, sentence, words))
			return true;
	}

	if (src[2]) {
		words.clear();

		words.push_back(src[0]);
		words.push_back(src[2]);

		if (handle_sentence(5, sentence, words))
			return true;
	}

	if (src[0]) {
		words.clear();
		words.push_back(src[0]);

		if (handle_sentence(6, sentence, words))
			return true;
	}

	return false;
}

bool ComprehendGame::handle_sentence(uint tableNum, Sentence *sentence, Common::Array<byte> &words) {
	const ActionTable &table = _actions[tableNum];

	for (uint i = 0; i < table.size(); i++) {
		const Action &action = table[i];

		// Check for a match on the words of the action
		bool isMatch = true;
		for (uint idx = 0; idx < action._nr_words && isMatch; ++idx)
			isMatch = action._words[idx] == words[idx];

		if (isMatch) {
			// Match
			_functionNum = action._function;
			return true;
		}
	}

	// No matching action
	return false;
}

void ComprehendGame::handleAction(Sentence *sentence) {
	_specialOpcode = 0;

	if (_functionNum == 0) {
		console_println(stringLookup(STRING_DONT_UNDERSTAND).c_str());
	} else {
		eval_function(_functionNum, sentence);
		_functionNum = 0;
		eval_function(0, nullptr);
	}

	handleSpecialOpcode();
}

Common::String ComprehendGame::expandLetterShortcut(char letter) {
	// Each shortcut maps to an ordered list of candidate full words; the first
	// that resolves to a real dictionary entry wins, covering the differing
	// truncations games store (e.g. "invent" vs "inv"). If none resolve, the
	// original single letter is returned unchanged.
	const char *candidates[3] = { nullptr, nullptr, nullptr };
	switch (tolower((unsigned char)letter)) {
	case 'i': candidates[0] = "inventory"; candidates[1] = "inv"; break;
	case 'x': candidates[0] = "examine";   candidates[1] = "exam"; break;
	case 'z': candidates[0] = "wait";      break;
	default:  return Common::String(letter);
	}

	for (uint i = 0; i < ARRAY_SIZE(candidates); ++i)
		if (candidates[i] && dict_find_word_by_string(this, candidates[i]))
			return candidates[i];

	return Common::String(letter);
}

void ComprehendGame::read_sentence(Sentence *sentence) {
	bool sentence_end = false;
	const char *word_string, *p = &_inputLine[_inputLineIndex];
	Word *word;

	sentence->clear();

	// V2 interpreters zero the input-number register (variable 0) at the start
	// of each sentence parse (NOVEL.EXE read_sentence_format writes 0x34bb = 0).
	// A subsequent numeric token fills it; otherwise it stays 0. This matters
	// for e.g. the curio dealer's yes/no haggling, whose accept test compares
	// variable 0 against 0. (V1 instead uses variable 0 as the persisted
	// inventory weight, so it must not be cleared.)
	if (hasNumberRegister())
		_variables[0] = 0;

	for (;;) {
		// Get the next word
		skip_whitespace(&p);
		word_string = p;
		skip_non_whitespace(&p);

		Common::String wordStr(word_string, p);

		// A purely numeric token is the player entering a number (e.g. a price
		// when haggling in Talisman's bazaar). The original interpreter parses
		// it into variable 0 -- its input-number register -- rather than
		// treating it as a dictionary word. Game logic and the number-mode '@'
		// string marker then read the value back from the variables array.
		bool allDigits = hasNumberRegister() && !wordStr.empty();
		for (uint di = 0; di < wordStr.size(); ++di) {
			if (wordStr[di] < '0' || wordStr[di] > '9') {
				allDigits = false;
				break;
			}
		}
		if (allDigits) {
			uint value = 0;
			for (uint di = 0; di < wordStr.size() && di < 5; ++di)
				value = value * 10 + (wordStr[di] - '0');
			_variables[0] = (uint16)value;
		}

		// Check for end of sentence
		// FIXME: The below is a hacked simplified version of how the
		// original handles cases like "get item1, item2"
		if (*p == ',' || *p == '\n' || wordStr.equalsIgnoreCase("and")) {
			// Sentence separator
			++p;
			sentence_end = true;
		} else if (*p == '\0') {
			sentence_end = true;
		}

		// Single-letter shortcuts for modern-IF conventions. The Comprehend data
		// makes a bare "I" a synonym for the "IN" direction, which surprises
		// players who expect "I" = inventory; "X" and "Z" aren't dictionary words
		// at all. Expand each to its full verb -- but only when this game's
		// dictionary actually defines it, so the token is left untouched (and the
		// original "IN" behaviour preserved) in any game that lacks the verb.
		if (wordStr.size() == 1)
			wordStr = expandLetterShortcut(wordStr[0]);

		/* Find the dictionary word for this */
		word = dict_find_word_by_string(this, wordStr.c_str());
		if (!word)
			sentence->_words[sentence->_nr_words].clear();
		else
			sentence->_words[sentence->_nr_words] = *word;

		sentence->_nr_words++;

		if (sentence->_nr_words >= ARRAY_SIZE(sentence->_words) ||
		        sentence_end)
			break;
	}

	parse_sentence_word_pairs(sentence);
	sentence->format();

	// Latch the command's first-noun flags so they persist through this turn's
	// function/daemon opcode execution (the article picker runs later, and may
	// run with a null sentence).  Faithful to the Apple II global $5b6a.
	_wordFlags = sentence->_wordFlags;

	_inputLineIndex = p - _inputLine;
}

void ComprehendGame::parse_sentence_word_pairs(Sentence *sentence) {
	if (sentence->_nr_words < 2)
		return;

	// Iterate through the pairs
	for (uint idx = 0; idx < _wordMaps.size(); ++idx) {
		for (int firstWord = 0; firstWord < (int)sentence->_nr_words - 1; ++firstWord) {
			for (int secondWord = firstWord + 1; secondWord < (int)sentence->_nr_words; ) {
				if (sentence->_words[firstWord] == _wordMaps[idx]._word[0] &&
					sentence->_words[secondWord] == _wordMaps[idx]._word[1]) {
					// Found a word pair match
					// Delete the second word
					for (; secondWord < (int)sentence->_nr_words - 1; ++secondWord)
						sentence->_words[secondWord] = sentence->_words[secondWord + 1];

					sentence->_words[sentence->_nr_words - 1].clear();
					sentence->_nr_words--;

					// Replace the first word with the target
					sentence->_words[firstWord] = _wordMaps[idx]._word[2];
				} else {
					// Move to next word
					++secondWord;
				}
			}
		}
	}
}

void ComprehendGame::doBeforeTurn() {
	// Make  a copy of the current room
	_currentRoomCopy = _currentRoom;
	_roomBeforeTurn = _currentRoom;

	beforeTurn();

	if (!_ended)
		update();
}

void ComprehendGame::beforeTurn() {
	// Run the each turn functions
	eval_function(0, nullptr);
}

void ComprehendGame::read_input() {
	Sentence tempSentence;
	bool handled;

turn:
	doBeforeTurn();
	if (_ended)
		return;

	beforePrompt();

	// Snapshot the state the player is about to act from, as a turn boundary
	// for #undo. (Deduplicated, so re-running the turn doesn't add a level.)
	g_comprehend->pushUndo();

	for (;;) {
		_redoLine = REDO_NONE;
		g_comprehend->print("> ");
		g_comprehend->readLine(_inputLine, INPUT_LINE_SIZE);
		if (g_comprehend->shouldQuit())
			return;

		// #quit metacommand: force-quit regardless of the game's own quit
		// handling (Talisman, for one, won't let the player quit normally).
		if (scumm_stricmp(_inputLine, "#quit") == 0) {
			g_comprehend->quitGame();
			return;
		}

		// #help: list and explain the # metacommands the interpreter handles
		// itself, on top of the game's own verbs. The graphics toggles depend on
		// the game, so only advertise them when this game actually supports them.
		if (scumm_stricmp(_inputLine, "#help") == 0) {
			g_comprehend->print(
				"\nThese commands are handled by the interpreter, not the game:\n"
				"\n"
				"  #undo (or UNDO)       Take back the last turn.\n"
				"  #restart              Start the game over from the beginning.\n"
				"  #save                 Save the game without taking a turn.\n"
				"  #restore              Restore a saved game without taking a turn.\n"
				"  #quit                 Stop playing and leave.\n"
				"  #transcript [on|off]  Record the game text to a file.\n");
			if (Common::DiskImageFS::active() && gmDhgrHaveDrawingTables())
				g_comprehend->print(
					"  #dhgr [on|off]        Switch between standard and double "
					"hi-res Apple II graphics.\n");
			if (gmpcjrHaveDrawingTables())
				g_comprehend->print(
					"  #pcjr [on|off]        Switch between CGA and PCjr "
					"16-colour graphics.\n");
			g_comprehend->print("  #help                 Show this list.\n");
			continue;
		}

		// #transcript [on|off]: echo game output to a text file.
		if (scumm_strnicmp(_inputLine, "#transcript", 11) == 0 &&
			(_inputLine[11] == '\0' || _inputLine[11] == ' ')) {
			g_comprehend->transcript(_inputLine + 11);
			continue;
		}

		// #undo (or plain "undo"): revert to the previous turn's state. The
		// undo snapshot taken above mirrors the current state, so this re-prompts
		// without advancing time; refresh the room display afterwards.
		if (scumm_stricmp(_inputLine, "#undo") == 0 ||
			scumm_stricmp(_inputLine, "undo") == 0) {
			if (g_comprehend->undo()) {
				g_comprehend->print("Move undone.\n");
				_updateFlags = (uint)(UPDATE_ROOM_DESC | UPDATE_GRAPHICS);
				update();
			} else {
				g_comprehend->print("You can't undo any further.\n");
			}
			continue;
		}

		// #restart: reload the game from the start, bypassing the in-game
		// restart prompt (Talisman, for one, won't let the player restart
		// normally). Clear the buffer for a clean slate and re-run the turn.
		if (scumm_stricmp(_inputLine, "#restart") == 0) {
			restartGame();
			glk_window_clear(g_comprehend->_bottomWindow);
			goto turn;
		}

		// #save / #restore: save or restore via the standard Glk file prompt,
		// bypassing the game's own SAVE/RESTORE verbs. The in-game verbs run as
		// ordinary turns, so a per-turn function fires around them; in Coveted
		// Mirror that function throws the player in jail at the start of the
		// game, clobbering any restored state. These metacommands talk straight
		// to the interpreter's save machinery, so no game turn elapses.
		if (scumm_stricmp(_inputLine, "#save") == 0) {
			(void)g_comprehend->saveGamePrompt();
			continue;
		}
		if (scumm_stricmp(_inputLine, "#restore") == 0) {
			if (g_comprehend->loadGamePrompt().getCode() == Common::kNoError) {
				// loadFromFileref already set UPDATE_ALL; repaint so the
				// restored room and picture show without waiting for a turn.
				update();
			}
			continue;
		}

		// #dhgr [on|off]: toggle the Apple II double hi-res ("<D>") renderer,
		// mirroring the original's Standard/Double hi-res prompt. Available only
		// on an Apple disk whose boot disk shipped a <D> interpreter (T5 drawing
		// tables installed); reports unavailable otherwise. On a switch we redraw
		// the current picture so the change shows immediately.
		if (scumm_strnicmp(_inputLine, "#dhgr", 5) == 0 &&
			(_inputLine[5] == '\0' || _inputLine[5] == ' ')) {
			if (!Common::DiskImageFS::active() || !gmDhgrHaveDrawingTables()) {
				g_comprehend->print("Double hi-res is not available for this game.\n");
				continue;
			}
			const char *arg = _inputLine + 5;
			while (*arg == ' ') ++arg;
			bool on;
			if (scumm_stricmp(arg, "on") == 0 || scumm_stricmp(arg, "double") == 0)
				on = true;
			else if (scumm_stricmp(arg, "off") == 0 || scumm_stricmp(arg, "standard") == 0)
				on = false;
			else
				on = !g_comprehend->dhgrMode();
			g_comprehend->setDhgrMode(on);
			g_comprehend->print(on ? "Double hi-res graphics on.\n"
			                       : "Standard hi-res graphics on.\n");
			_updateFlags = (uint)UPDATE_GRAPHICS;
			update();
			continue;
		}

		// #pcjr [on|off]: toggle the IBM PCjr 16-colour renderer for the DOS v1
		// games (Transylvania v1 / Crimson Crown), mirroring the original
		// NOVEL.EXE's PCjr branch (BIOS mode 9). Available only when the
		// JR_GRAPH.OVR drawing tables loaded; reports unavailable otherwise.
		if (scumm_strnicmp(_inputLine, "#pcjr", 5) == 0 &&
			(_inputLine[5] == '\0' || _inputLine[5] == ' ')) {
			if (!gmpcjrHaveDrawingTables()) {
				g_comprehend->print("PCjr graphics are not available for this game.\n");
				continue;
			}
			const char *arg = _inputLine + 5;
			while (*arg == ' ') ++arg;
			bool on;
			if (scumm_stricmp(arg, "on") == 0)
				on = true;
			else if (scumm_stricmp(arg, "off") == 0 || scumm_stricmp(arg, "cga") == 0)
				on = false;
			else
				on = !g_comprehend->pcjrMode();
			g_comprehend->setPcjrMode(on);
			g_comprehend->print(on ? "PCjr 16-colour graphics on.\n"
			                       : "CGA graphics on.\n");
			_updateFlags = (uint)UPDATE_GRAPHICS;
			update();
			continue;
		}

		_inputLineIndex = 0;
		if (strlen(_inputLine) == 0) {
//			// Empty line, so toggle picture window visibility
//			g_comprehend->toggleGraphics();
//			updateRoomDesc();
//			g_comprehend->print(_("Picture window toggled\n"));
//
//			_updateFlags |= UPDATE_GRAPHICS;
//			update_graphics();
			continue;
		}

		afterPrompt();

		if (_redoLine == REDO_NONE)
			break;
		else if (_redoLine == REDO_TURN)
			goto turn;
	}

	for (;;) {
		NounState prevNounState = _nounState;
		_nounState = NOUNSTATE_STANDARD;

		read_sentence(&tempSentence);

		// Carry the previous turn's verb over only when the new input is a bare
		// noun (the "reuse the last verb" feature). If it parsed to no recognised
		// words at all, replace the sentence wholesale instead of keeping the old
		// verb -- otherwise an unintelligible command silently re-runs the last
		// one (e.g. repeating "Ye cannot travel..." ) rather than reporting that
		// it was not understood.
		bool newHasWords = false;
		for (uint fw = 0; fw < ARRAY_SIZE(tempSentence._formattedWords); ++fw)
			if (tempSentence._formattedWords[fw]) {
				newHasWords = true;
				break;
			}

		// After OPCODE_SAVE_ACTION (NOUNSTATE_QUERY) the next input re-uses the
		// previous command's verb, so a bare answer with no verb of its own is
		// matched as "<saved verb> <answer>". This is how Talisman's shop clerk
		// reads the 5-digit product code: the verb stays the product-prefix word
		// the player gave (e.g. "cp"), and entering just "65013" re-runs that
		// product's handler -- which, with its prompt flag now set, prints "A
		// fine choice." rather than rejecting the input. Keep the new verb when
		// the answer does carry one (e.g. a "yes"/"no" reply to a yes/no query).
		bool copyNoun;
		if (prevNounState == NOUNSTATE_QUERY)
			copyNoun = tempSentence._formattedWords[0] != 0;
		else
			copyNoun = tempSentence._formattedWords[0] ||
				prevNounState != NOUNSTATE_STANDARD || !newHasWords;
		_sentence.copyFrom(tempSentence, copyNoun);

		handled = handle_sentence(&_sentence);
		handleAction(&_sentence);

		if (!handled)
			return;

		/* FIXME - handle the 'before you can continue' case */
		if (_inputLine[_inputLineIndex] == '\0')
			break;
	}

	afterTurn();
}

void ComprehendGame::playGame() {
	if (!g_comprehend->loadLauncherSavegameIfNeeded())
		beforeGame();

	_updateFlags = (uint)UPDATE_ALL;
	while (!g_comprehend->shouldQuit()) {
		read_input();

		if (_ended && !handle_restart())
			break;
	}
}

uint ComprehendGame::getRandomNumber(uint max) const {
	return g_comprehend->getRandomNumber(max);
}

void ComprehendGame::doMovementVerb(uint verbNum) {
	assert(verbNum >= 1 && verbNum <= NR_DIRECTIONS);
	Room *room = get_room(_currentRoom);
	byte newRoom = room->_direction[verbNum - 1];

	if (newRoom)
		move_to(newRoom);
	else
		console_println(_strings[0].c_str());
}

void ComprehendGame::weighInventory() {
	_totalInventoryWeight = 0;
	if (!g_debugger->_invLimit)
		// Allow for an unlimited number of items in inventory
		return;

	for (int idx = _itemCount - 1; idx > 0; --idx) {
		Item *item = get_item(idx);
		if (item->_room == ROOM_INVENTORY)
			_totalInventoryWeight += item->_flags & ITEMF_WEIGHT_MASK;
	}
}

} // namespace Comprehend
} // namespace Glk

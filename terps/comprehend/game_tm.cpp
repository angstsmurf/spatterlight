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
#include "game_tm.h"
#include "pics.h"
#include "comprehend_compat.h"

namespace Glk {
namespace Comprehend {

TalismanGame::TalismanGame() : ComprehendGameV2() {
	_gameDataFile = "G0";

	addLetteredFiles(_locationGraphicFiles, "R", 'A', 'G');
	// Item art is numbered against OA..OF ((graphic-1) / 16 picks the file),
	// but OC and OD are absent. They must still occupy slots 2 and 3 so OE/OF
	// stay at slots 4/5; Pics::load() substitutes an empty placeholder.
	addLetteredFiles(_itemGraphicFiles, "O", 'A', 'F');

	_titleGraphicFile = "t0";

	if (Common::DiskImageFS::active()) {
		// Apple II: the game starts on the "Empire" disk (g0 magic 0xa429).
		// Graphics span all three disks (RA-RD + OA/OB on the Empire disk,
		// RE-RG + OE/OF on the Lands Beyond disk) and resolve through the
		// active-disk-then-fallback VFS. The title (T0, on the boot disk) is a
		// Graphics Magician vector image like the rooms, not a raw hi-res screen.
		Common::DiskImageFS::selectDiskByGameMagic(0xa429);
	}
}

bool TalismanGame::prepareRestore(const std::vector<byte> &payload) {
	// Talisman is two games in one: part 2 (the desert) is a separate database
	// with its own room/item/function tables, dictionary, '@'-replacement words
	// and string banks. A saved game stores only the dynamic state (rooms,
	// items, flags, variables), so swap in the database of the save's part
	// before the state is read into it. The part is the last byte of the
	// payload (see synchronizeSave).
	if (payload.empty() || payload.back() > 1)
		return false;

	const bool part2 = payload.back() != 0;
	if (part2 == _inPart2)
		return true;

	_inPart2 = part2;
	if (Common::DiskImageFS::active())
		Common::DiskImageFS::selectDiskByGameMagic(part2 ? 0x94c6 : 0xa429);
	else
		_gameDataFile = part2 ? "H0" : "G0";
	loadGame();      // that part's rooms/items/functions/dictionary/replace-words
	loadStrings();   // that part's string banks
	return true;
}

void TalismanGame::synchronizeSave(Common::Serializer &s) {
	ComprehendGameV2::synchronizeSave(s);

	byte part2 = _inPart2 ? 1 : 0;
	s.syncAsByte(part2);
}

void TalismanGame::upgradeSavePayload(std::vector<byte> &payload, byte version) {
	if (version >= 3)
		return;

	// Older saves don't record the part. Flag 79, the moving-wall maze armed by
	// part 2's func 44, is the best guess they allow: it is clear at the end of
	// part 1, but also clear in parts of the desert. Those saves still need part
	// 2 to be loaded already, as they always did.
	// Payload layout (see ComprehendGame::synchronizeSave): currentRoom (2) +
	// variables (MAX_VARIABLES * 2) + flags (MAX_FLAGS).
	const uint flag79Off = 2 + MAX_VARIABLES * 2 + 79;
	const bool part2 = _inPart2 || (payload.size() > flag79Off && payload[flag79Off]);
	payload.push_back(part2 ? 1 : 0);
}

void TalismanGame::playGame() {
	loadStrings();
	ComprehendGameV2::playGame();
}

void TalismanGame::beforeGame() {
	g_comprehend->drawPicture(TITLE_IMAGE);

	// Print game information, centred as on the original title screen.
	g_comprehend->setCentered(true);
	console_println("Story by Bruce X. Hoffman");
	console_println("Graphics by Ray Redlich and Brian Poff");
	console_println("Project managed and IBM version by");
	console_println("Jeffrey A. Jay");
	console_println("Copyright 1987  POLARWARE, Inc.");
	g_comprehend->setCentered(false);
	g_comprehend->readChar();

	glk_window_clear(g_comprehend->_bottomWindow);
}

void TalismanGame::beforeTurn() {
	_variables[0x62] = g_vm->getRandomNumber(255);

	_functionNum = 17;
	handleAction(nullptr);
}

void TalismanGame::beforePrompt() {
	_functionNum = 14;
	handleAction(nullptr);

#ifdef COMPREHEND_DEBUG
	static const bool trace = getenv("TM_TRACE") != nullptr;
	if (trace) {
		Common::String held;
		for (uint i = 0; i < _items.size(); ++i)
			if (_items[i]._room == ROOM_INVENTORY)
				held += Common::String::format("%u ", i);
		Common::String vars;
		for (uint i = 0; i < MAX_VARIABLES; ++i)
			if (_variables[i]) vars += Common::String::format("%u:%u ", i, _variables[i]);
		Common::String flags;
		for (uint i = 0; i < MAX_FLAGS; ++i)
			if (_flags[i]) flags += Common::String::format("%u ", i);
		fprintf(stderr, "[TM_TRACE] room=%u var72=%u var98=%u held={ %s} vars={ %s} flags={ %s}\n",
			_currentRoom, _variables[72], _variables[0x62], held.c_str(), vars.c_str(), flags.c_str());
	}
#endif
}

void TalismanGame::afterPrompt() {
	if (_savedAction.empty()) {
		_functionNum = 19;
		handleAction(nullptr);
		if (_redoLine == REDO_NONE && _flags[3])
			_redoLine = REDO_PROMPT;
	} else {
		Common::strcpy_s(_inputLine, _savedAction.c_str());
		_savedAction.clear();
	}
}

void TalismanGame::handleAction(Sentence *sentence) {
	// The pending-question redirect (e.g. the curio dealer's yes/no haggling)
	// applies only to a matched verb action, gated by flag 62. The per-turn
	// hooks pass a null sentence and must not redirect: func 17 in beforeTurn
	// would jump to the answer handler (var126) before the player could answer.
	if (sentence && _flags[62] && _functionNum != _variables[125]) {
		_variables[124] = _functionNum;
		_functionNum = _variables[126];
	}

	ComprehendGameV2::handleAction(sentence);
}

void TalismanGame::handleSpecialOpcode() {
	switch (_specialOpcode) {
	case 2:
		deathMenu();
		break;

	case 3:
		specialSaveRoom();
		break;

	case 4:
		specialRestoreRoom();
		break;

	case 5:
		// The original's dispatcher has no case 5, though the bytecode emits
		// it. Listed only to suppress the unhandled-opcode warning.
		break;

	case 6:
		game_save();
		break;

	case 7:
		game_restore();
		break;

	case 9:
		// The original toggles a bit read only by its DOS picture interpreter
		// to pick a drawing parameter. This renderer has no analogue; handled
		// to avoid the unhandled-opcode warning.
		break;

	case 15:
		specialTextMode();
		break;

	case 16:
		specialBoatDeparture();
		break;

	case 17:
		specialGraphicsMode();
		break;

	case 20:
		specialNpcConversation();
		break;

	default:
		if (_specialOpcode != 0)
			warning("TalismanGame: unhandled special opcode %d", _specialOpcode);
		break;
	}
}

void TalismanGame::specialSaveRoom() {
	// Save the room the player started this turn in, before any move. Paired
	// with special 4, this lets the game move the player to a scripted scene
	// and back. The magic lamp moves the player inside and only then fires
	// special 3, so saving the live room or _currentRoomCopy (which move_to
	// overwrites) would trap the player in the lamp.
	_savedRoom = _roomBeforeTurn;
}

void TalismanGame::specialRestoreRoom() {
	// Return to the room saved by special 3.
	_currentRoom = _savedRoom;
	_updateFlags |= UPDATE_GRAPHICS | UPDATE_ROOM_DESC;
}

void TalismanGame::specialTextMode() {
	// The game flips text/graphics mode several times during its intro.
	// Graphics stay on here: text always renders in the buffer below the
	// picture, so blanking the picture would only cause churn.
	_functionNum = 19;
	handleAction(nullptr);
	_redoLine = REDO_TURN;
}

void TalismanGame::specialBoatDeparture() {
	// The player sails out of the Persian Empire into part 2 (the desert).
	// enterPart2() swaps in the desert database: the Lands Beyond disk's g0 on
	// the Apple II, or H0 plus novel.exe's second string segment on DOS. The
	// departure daemon (func 20) emits this special every turn while flag 45 is
	// set and never clears it. Only that block tests flag 45, in either part,
	// so clearing it here stops the scene repeating.
	if (!_inPart2) {
		if (!g_comprehend->isGraphicsEnabled())
			g_comprehend->setGraphicsMode(true);
		enterPart2();
	}
	_flags[45] = false;
	// enterPart2() sets _currentRoom directly, not via move_to, so arm the
	// update flags here; otherwise the first part-2 turn keeps the old room's
	// description and picture. The original likewise redraws on entering its
	// fresh main loop.
	_updateFlags |= UPDATE_ALL;
	update();
	_redoLine = REDO_TURN;
}

void TalismanGame::specialGraphicsMode() {
	if (!g_comprehend->isGraphicsEnabled())
		g_comprehend->setGraphicsMode(true);

	_updateFlags |= UPDATE_ALL;
	update();
	_redoLine = REDO_TURN;
}

void TalismanGame::specialNpcConversation() {
	// NPC-conversation trigger (the abu/executioner/clerk verb handlers, funcs
	// 37/114/261). The original runs function 16 (the dialogue printer) when
	// the NPC verb ends the line, and otherwise keeps the verb as a forced verb
	// for the next parse. NOUNSTATE_QUERY verb re-use already covers chained
	// commands, so only the function-16 path is needed.
	_functionNum = 16;
	handleAction(nullptr);
}

void TalismanGame::deathMenu() {
	// Death / game-over menu. The bytecode has already narrated the death and
	// printed "THE END." Wait for a key, then show the menu (main string table
	// entry 8) until the player chooses: '1' quit, '2' restart, '3' restore.
	// Without this the death re-fires every turn.
	g_comprehend->readChar();

	// A failed RESTORE returns to the menu rather than resuming the dead game.
	for (;;) {
		console_println(_strings[8].c_str());
		// The original menu only offers 1/2/3; 'U' is this interpreter's own
		// undo, on top of the game, mirroring the in-game #undo metacommand.
		// It steps back two turns: Talisman's deaths are delayed countdowns, so
		// undoing only the fatal turn usually lands the player in a state that
		// kills them again the moment they resume.
		console_println("(Or press U to undo the last couple of moves.)");

		int c = console_get_key();
		if (g_comprehend->shouldQuit())
			return;

		if (c == '2') {
			// Reload the initial game state, silently: the menu above has
			// already taken the player's choice.
			game_restart(true);
			return;
		} else if (c == '3') {
			// Resume play with the loaded state. If the restore fails, fall
			// through to re-show the menu.
			if (game_restore())
				return;
		} else if (c == 'u' || c == 'U') {
			// Take back the fatal move and the one before it (see undoTurn): two
			// turns gives the slack a delayed-countdown death needs. Refresh the
			// room and picture the way the #undo metacommand does. If there is
			// nothing to undo, fall through to re-show the menu.
			if (g_comprehend->undoTurn(2)) {
				_updateFlags = (uint)UPDATE_ALL;
				update();
				return;
			}
		} else {
			// '1', or anything else. Under transcript replay readChar()/
			// console_get_key() always return a blank, which lands here and
			// quits, so the loop can't spin.
			g_comprehend->quitGame();
			return;
		}
	}
}

void TalismanGame::restartGame() {
	ComprehendGame::restartGame();
	// loadGame() repopulates _strings from the G0 data file, but Talisman's
	// strings live in novel.exe / the MA-MO banks, so reload them as playGame()
	// does or every message decodes to BAD_STRING.
	loadStrings();
}

} // namespace Comprehend
} // namespace Glk

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

	_locationGraphicFiles.push_back("RA");
	_locationGraphicFiles.push_back("RB");
	_locationGraphicFiles.push_back("RC");
	_locationGraphicFiles.push_back("RD");
	_locationGraphicFiles.push_back("RE");
	_locationGraphicFiles.push_back("RF");
	_locationGraphicFiles.push_back("RG");
	_itemGraphicFiles.push_back("OA");
	_itemGraphicFiles.push_back("OB");
	_itemGraphicFiles.push_back("OE");
	_itemGraphicFiles.push_back("OF");

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

#define STRINGS_SEGMENT1 0x16490
#define STRINGS_SEGMENT2 0x22fa0
#define BANKS_COUNT 15
#define STRINGS_PER_BANK 64

void TalismanGame::loadStringsApple() {
	// On the Apple II the strings live in 15 standard string-table files
	// (MA-MO) instead of being embedded in novel.exe; MA-MN are on the boot
	// disk and MO on the Empire disk. Banks 0-7 fill _strings, 8-14 _strings2,
	// matching the novel.exe layout the bytecode references.
	static const char *const BANKS[BANKS_COUNT] = {
		"MA", "MB", "MC", "MD", "ME", "MF", "MG", "MH",
		"MI", "MJ", "MK", "ML", "MM", "MN", "MO"
	};

	_strings.clear();
	_strings2.clear();

	for (int bank = 0; bank < BANKS_COUNT; ++bank) {
		StringTable &table = (bank < 8) ? _strings : _strings2;

		// Each Apple II string file uses the same layout as one novel.exe
		// bank: a table of LE16 string offsets (relative to the start of the
		// file) followed by the 5-bit-encoded string data. The index begins
		// at the very start of the file, so the first two entries are the two
		// standard parser messages ("Ye cannot travel in that direction.",
		// "I understand thee not."), exactly matching the DOS bank.
		FileBuffer fb(BANKS[bank]);
		fb.seek(0);
		uint fileSize = fb.size();

		uint16 index[STRINGS_PER_BANK];
		Common::fill(&index[0], &index[STRINGS_PER_BANK], 0);
		for (int i = 0; i < STRINGS_PER_BANK; ++i) {
			uint v = fb.readUint16LE();
			if (v > fileSize)
				break;
			index[i] = v;
		}

		for (int i = 0; i < STRINGS_PER_BANK; ++i) {
			if (index[i]) {
				fb.seek(index[i]);
				table.push_back(parseString(&fb));
			} else {
				table.push_back("");
			}
		}
	}
}

void TalismanGame::loadStrings() {
	if (Common::DiskImageFS::active()) {
		loadStringsApple();
		return;
	}

	int bankOffsets[BANKS_COUNT];
	int stringOffsets[STRINGS_PER_BANK + 1];

	_strings.clear();
	_strings2.clear();

	Common::File f;
	if (!f.open("novel.exe"))
		error("novel.exe is a required file");

	Common::String md5 = Common::computeStreamMD5AsString(f, 1024);
	if (md5 != "0e7f002971acdb055f439020363512ce" && md5 != "2e18c88ce352ebea3e14177703a0485f")
		warning("Unrecognised novel.exe encountered; strings may be incorrect");

	const int STRING_SEGMENTS[2] = { STRINGS_SEGMENT1, STRINGS_SEGMENT2 };

	// TODO: Figure out use of string segment 2
	for (int strings = 0; strings < 1; ++strings) {
		f.seek(STRING_SEGMENTS[strings]);
		for (int bank = 0; bank < BANKS_COUNT; ++bank)
			bankOffsets[bank] = f.readUint16LE();

		// Iterate through the banks loading the strings
		for (int bank = 0; bank < BANKS_COUNT; ++bank) {
			if (!bankOffsets[bank])
				continue;

			f.seek(STRING_SEGMENTS[strings] + bankOffsets[bank]);
			for (int strNum = 0; strNum <= STRINGS_PER_BANK; ++strNum)
				stringOffsets[strNum] = f.readUint16LE();

			for (int strNum = 0; strNum < STRINGS_PER_BANK; ++strNum) {
				int size = stringOffsets[strNum + 1] - stringOffsets[strNum];
				if (size < 0)
					size = 0xfff;

				f.seek(STRING_SEGMENTS[strings] + bankOffsets[bank] + stringOffsets[strNum]);
				FileBuffer fb(&f, size);
				Common::String str = parseString(&fb);

				if (bank < 8)
					_strings.push_back(str);
				else
					_strings2.push_back(str);
			}
		}
	}
}

void TalismanGame::playGame() {
	loadStrings();
	ComprehendGameV2::playGame();
}

void TalismanGame::beforeGame() {
	// Draw the title
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
	// The pending-question redirect (used by e.g. the curio dealer's yes/no
	// haggling) must only apply to the matched verb action, never to the
	// per-turn hook functions (beforeTurn/beforePrompt/afterPrompt, which pass
	// a null sentence). In NOVEL.EXE the redirect lives in game_turn_loop
	// solely between match_action_tables (1000:0665) and the verb execute
	// (1000:0681 CALL 0x0693), gated by flag 62; the hooks are invoked
	// separately via routine at 1000:0690 without it. Applying it to the hooks
	// made the next turn's beforeTurn (func 17) jump straight to the answer
	// handler (var126), firing the "no" outcome before the player could answer.
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
		// Save the current room (NOVEL.EXE FUN_1000_06f0 case 3:
		// [0x9591] = [0x9592], where 0x9592 mirrors the start-of-turn room).
		// Paired with special 4, this lets the game whisk the player away for a
		// scripted scene and then return them to where they were.
		_savedRoom = _currentRoomCopy;
		break;

	case 4:
		// Return to the room saved by special 3 (NOVEL.EXE case 4:
		// currentRoom = [0x9591]).
		_currentRoom = _savedRoom;
		_updateFlags |= UPDATE_GRAPHICS | UPDATE_ROOM_DESC;
		break;

	case 5:
		// NOVEL.EXE's dispatcher has no case 5; the bytecode emits it but the
		// original interpreter does nothing, so this is a faithful no-op (it is
		// listed explicitly only to suppress the unhandled-opcode warning).
		break;

	case 6:
		game_save();
		break;

	case 7:
		game_restore();
		break;

	case 9:
		// NOVEL.EXE case 9 toggles bit 0 of an interpreter-internal byte
		// (0x9d5b) that is read only by the DOS picture interpreter
		// (1000:2e25) to pick a drawing parameter. It has no analogue in our
		// renderer, so this is a deliberate no-op (handled to avoid the
		// unhandled-opcode warning).
		break;

	case 15:
		// "Switch to text screen mode." We deliberately keep graphics on and
		// leave the picture window visible: the game flips text/graphics mode
		// several times during its intro, and since our text always renders in
		// the scrolling buffer below the picture, blanking the picture just
		// caused distracting churn. Only the function call below matters here.
		_functionNum = 19;
		handleAction(nullptr);
		_redoLine = REDO_TURN;
		break;

	case 16:
		// Scripted scene (NOVEL.EXE case 16): move to room 0x33 (51), make sure
		// graphics are on, then run function 44 in that room and redo the turn,
		// the same shape as the text/graphics-mode specials above.
		_currentRoom = 51;
		if (!g_comprehend->isGraphicsEnabled())
			g_comprehend->setGraphicsMode(true);
		_functionNum = 44;
		handleAction(nullptr);
		_redoLine = REDO_TURN;
		break;

	case 17:
		// Switch to graphics mode
		if (!g_comprehend->isGraphicsEnabled())
			g_comprehend->setGraphicsMode(true);

		_updateFlags |= UPDATE_ALL;
		update();
		_redoLine = REDO_TURN;
		break;

	case 20:
		// NOVEL.EXE case 20 runs function 16 when the interpreter is in normal
		// turn mode ([0x35e7] == 2, which is the case whenever a special opcode
		// fires during play); the surrounding 0x9593/0x35db bookkeeping is
		// interpreter scratch that no bytecode reads, so only the function call
		// is observable.
		_functionNum = 16;
		handleAction(nullptr);
		break;

	default:
		if (_specialOpcode != 0)
			warning("TalismanGame: unhandled special opcode %d", _specialOpcode);
		break;
	}
}

void TalismanGame::deathMenu() {
	// Death / game-over menu (NOVEL.EXE special opcode 2, FUN_1000_06f0 case 2).
	// By the time this fires the bytecode has already narrated the death and
	// printed "THE END." The interpreter then switches to a text screen, waits
	// for a keypress, and loops showing the menu string (main string table
	// entry 8, right after the save/restore prompts) until the player chooses:
	//   '1' Quit, '2' "embark upon a new adventure" (restart), '3' Restore.
	// Without this the per-turn death just keeps re-firing, replaying the last
	// dying turn forever.
	g_comprehend->readChar();

	// The original loops on the menu string until the player makes a valid
	// choice. We re-show it the same way after a failed RESTORE, so a cancelled
	// or unreadable save drops back to the menu rather than silently resuming
	// the just-dead game.
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
			// Reload the initial game state. handle_restart() does the reload
			// without the generic restart prompt, since the menu above has
			// already taken the player's choice.
			game_restart();
			return;
		} else if (c == '3') {
			// FUN_1000_0785: same prompt and slot handling as RESTORE, then
			// resume play with the loaded state. If the restore fails, fall
			// through to re-show the menu instead of resuming the dead game.
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
	// loadGame() repopulates _strings from the G0 game-data file, but
	// Talisman's strings actually live in novel.exe / the MA-MO bank files, so
	// reload them the same way playGame() does -- otherwise every message after
	// a restart decodes to BAD_STRING.
	loadStrings();
}

bool TalismanGame::handle_restart() {
	// Reached only from the death menu's "new adventure" choice, which has
	// already prompted the player, so reload silently instead of asking again.
	restartGame();
	return true;
}

} // namespace Comprehend
} // namespace Glk

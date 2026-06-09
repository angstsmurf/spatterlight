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
		// active-disk-then-fallback VFS. The title is a raw hi-res screen.
		Common::DiskImageFS::selectDiskByGameMagic(0xa429);
		_titleGraphicFile = "";
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
		error("Unrecognised novel.exe encountered");

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

	// Print game information
	console_println("Story by Bruce X.Hoffman. Graphics by Ray Redlich and Brian Poff");
	console_println("Project managed and IBM version by Jeffrey A. Jay. "
		"Copyright 1987 POLARWARE Inc.");
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
	if (_flags[62] && _functionNum != _variables[125]) {
		_variables[124] = _functionNum;
		_functionNum = _variables[126];
	}

	ComprehendGameV2::handleAction(sentence);
}

void TalismanGame::handleSpecialOpcode() {
	switch (_specialOpcode) {
	case 15:
		// Switch to text screen mode
		if (g_comprehend->isGraphicsEnabled()) {
			g_comprehend->toggleGraphics();
			updateRoomDesc();
		}

		_functionNum = 19;
		handleAction(nullptr);
		_redoLine = REDO_TURN;
		break;

	case 17:
		// Switch to graphics mode
		if (!g_comprehend->isGraphicsEnabled())
			g_comprehend->toggleGraphics();

		_updateFlags |= UPDATE_ALL;
		update();
		_redoLine = REDO_TURN;
		break;

	default:
		break;
	}
}

} // namespace Comprehend
} // namespace Glk

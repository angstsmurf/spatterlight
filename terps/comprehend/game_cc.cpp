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

#include "game_cc.h"
#include "comprehend.h"
#include "pics.h"

namespace Glk {
namespace Comprehend {

static const GameStrings CC1_STRINGS = {0x9};

CrimsonCrownGame::CrimsonCrownGame() : ComprehendGameV1(),
		_diskNum(1), _newDiskNum(1) {
	setupDisk(1);
}

void CrimsonCrownGame::setupDisk(uint diskNum) {
	assert(diskNum == 1 || diskNum == 2);

	if (Common::DiskImageFS::active()) {
		// Apple II: both disks name their files identically (g0, MA, RA...),
		// so switch the active disk image by its g0 magic word. Disk one is
		// 0x84fe, disk two is 0x7dec. The title is a raw hi-res screen.
		Common::DiskImageFS::selectDiskByGameMagic(diskNum == 1 ? 0x84fe : 0x7dec);

		_gameDataFile = "g0";

		_stringFiles.clear();
		_stringFiles.push_back("MA");

		_locationGraphicFiles.clear();
		_locationGraphicFiles.push_back("RA");
		_locationGraphicFiles.push_back("RB");
		if (diskNum == 1)
			_locationGraphicFiles.push_back("RC");

		_itemGraphicFiles.clear();
		_itemGraphicFiles.push_back("OA");
		_itemGraphicFiles.push_back("OB");

		_gameStrings = (diskNum == 1) ? &CC1_STRINGS : nullptr;
		// The Apple II title ("Crimson Crown" + sun) is a Graphics Magician
		// vector image inside T0, after the disk-protection / loader stub;
		// Pics::ImageFile starts it at the right offset (0x100).
		_titleGraphicFile = "t0";
		_diskNum = diskNum;
		return;
	}

	_gameDataFile = Common::String::format("cc%u.gda", diskNum);

	_stringFiles.clear();
	_stringFiles.push_back(Common::String::format("ma.ms%u", diskNum).c_str());

	_locationGraphicFiles.clear();
	_locationGraphicFiles.push_back(Common::String::format("ra.ms%u", diskNum));
	_locationGraphicFiles.push_back(Common::String::format("rb.ms%u", diskNum));
	if (diskNum == 1)
		_locationGraphicFiles.push_back("RC.ms1");

	_itemGraphicFiles.clear();
	_itemGraphicFiles.push_back(Common::String::format("oa.ms%u", diskNum));
	_itemGraphicFiles.push_back(Common::String::format("ob.ms%u", diskNum));

	if (diskNum == 1)
		_gameStrings = &CC1_STRINGS;
	else
		_gameStrings = nullptr;

	_titleGraphicFile = "cctitle.ms1";
	_diskNum = diskNum;
}

void CrimsonCrownGame::beforeGame() {
	// Draw the title
	g_comprehend->drawPicture(TITLE_IMAGE);
	g_comprehend->readChar();
}

void CrimsonCrownGame::synchronizeSave(Common::Serializer &s) {
	if (s.isSaving()) {
		s.syncAsByte(_diskNum);
	} else {
		// Get the disk the save is for. The beforeTurn call allows
		// for the currently loaded disk to be switched if necessary
		s.syncAsByte(_newDiskNum);
		beforeTurn();
	}

	ComprehendGame::synchronizeSave(s);
}

void CrimsonCrownGame::handleSpecialOpcode() {
	switch (_specialOpcode) {
	case 1:
		// Crystyal ball cutscene
		if (_diskNum == 1) {
			crystalBallCutscene();
		} else {
			throneCutscene();
		}
		break;

	case 3:
		// Game over - failure
		game_restart();
		break;

	case 5:
		if (_diskNum == 1) {
			// Finished disk 1
			g_comprehend->readChar();
			g_comprehend->drawLocationPicture(41);
			console_println(_strings2[26].c_str());
			g_comprehend->readChar();

			_newDiskNum = 2;
			move_to(21);
			console_println(_strings[407].c_str());

		} else {
			// Won the game
			g_comprehend->drawLocationPicture(29, false);
			g_comprehend->drawItemPicture(20);
			console_println(stringLookup(0x21c).c_str());
			console_println(stringLookup(0x21d).c_str());

			g_comprehend->readChar();
			g_comprehend->quitGame();
		}
		break;

	case 6:
		game_save();
		break;

	case 7:
		game_restore();
		break;

	default:
		break;
	}
}

void CrimsonCrownGame::crystalBallCutscene() {
	g_comprehend->showGraphics();

	for (int screenNum = 38; screenNum <= 40; ++screenNum) {
		g_comprehend->drawLocationPicture(screenNum);
		g_comprehend->readChar();
		if (g_comprehend->shouldQuit())
			return;
	}
}

void CrimsonCrownGame::throneCutscene() {
	// Show the screen
	update();
	console_println(stringLookup(0x20A).c_str());

	// Handle what happens in climatic showdown
	eval_function(14, nullptr);
}

void CrimsonCrownGame::beforePrompt() {
	// Clear the Sabrina/Erik action flags
	_flags[0xa] = 0;
	_flags[0xb] = 0;
}

void CrimsonCrownGame::beforeTurn() {
	if (_newDiskNum != _diskNum) {
		setupDisk(_newDiskNum);
		loadGame();
		move_to(_currentRoom);
	}

	// Do NOT run the each-turn daemon (function 0) here. RE-verified against the
	// V1 DOS interpreter (crimson-crown/NOVEL.EXE): the main loop FUN_1000_0455
	// displays the room at the top, then parses and dispatches; the dispatch
	// FUN_1000_0e73 evaluates the matched action and then function 0 -- the daemon
	// -- exactly ONCE (index set to 0, evaluator FUN_1000_0eb4 called again; that
	// evaluator has no other caller), with no daemon pass before the parser.
	// Calling ComprehendGameV1::beforeTurn() (-> ComprehendGame::beforeTurn() ->
	// eval_function(0)) ran the daemon a second time, doubling every per-turn
	// function-0 effect. (Same fix as the V2 games; see git 845e9d5d / 2da5bb81 /
	// dad6272a.)
}

bool CrimsonCrownGame::handle_restart() {
	if (_diskNum != 1) {
		setupDisk(1);
		loadGame();
	}

	return ComprehendGame::handle_restart();
}

void CrimsonCrownGame::restartGame() {
	// The game spans multiple disks; a restart must return to disk 1 first.
	if (_diskNum != 1) {
		setupDisk(1);
		loadGame();
	}
	ComprehendGame::restartGame();
}

} // namespace Comprehend
} // namespace Glk

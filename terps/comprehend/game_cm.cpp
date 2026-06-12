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

#include "game_cm.h"
#include "comprehend.h"

namespace Glk {
namespace Comprehend {

// TODO: the restart-prompt string index is a placeholder; verify the real
// value (and the rest of the extra-string banks) once the game is play-tested.
static const GameStrings CM_STRINGS = {
	EXTRA_STRING_TABLE(0)
};

CovetedMirrorGame::CovetedMirrorGame() : ComprehendGameV2() {
	_gameDataFile = "g0";

	// The Apple II release keeps its canned messages in twelve separate string
	// banks (MA-ML), the same way OO-Topos uses MA-ME. Each holds up to 64
	// strings, indexed via EXTRA_STRING_TABLE (0x8200 | n).
	_stringFiles.push_back("MA");
	_stringFiles.push_back("MB");
	_stringFiles.push_back("MC");
	_stringFiles.push_back("MD");
	_stringFiles.push_back("ME");
	_stringFiles.push_back("MF");
	_stringFiles.push_back("MG");
	_stringFiles.push_back("MH");
	_stringFiles.push_back("MI");
	_stringFiles.push_back("MJ");
	_stringFiles.push_back("MK");
	_stringFiles.push_back("ML");

	// Disk #1 carries the room (RA-RG) and object (OA-OG) pictures.
	_locationGraphicFiles.push_back("RA");
	_locationGraphicFiles.push_back("RB");
	_locationGraphicFiles.push_back("RC");
	_locationGraphicFiles.push_back("RD");
	_locationGraphicFiles.push_back("RE");
	_locationGraphicFiles.push_back("RF");
	_locationGraphicFiles.push_back("RG");

	_itemGraphicFiles.push_back("OA");
	_itemGraphicFiles.push_back("OB");
	_itemGraphicFiles.push_back("OC");
	_itemGraphicFiles.push_back("OD");
	_itemGraphicFiles.push_back("OE");
	_itemGraphicFiles.push_back("OF");
	_itemGraphicFiles.push_back("OG");

	_colorTable = 1;
	_gameStrings = &CM_STRINGS;

	// The title is a Graphics Magician vector image in T0 (like OO-Topos).
	_titleGraphicFile = "t0";
}

} // namespace Comprehend
} // namespace Glk

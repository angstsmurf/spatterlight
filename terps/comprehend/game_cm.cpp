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
#include "pics.h"

// Engine-side behaviour here is reverse-engineered from the Apple II
// interpreter.

namespace Glk {
namespace Comprehend {

// The banks contain no restart-prompt string (the original halts on THE END);
// restartPrompt() below returns a literal instead, so this entry is unused.
static const GameStrings CM_STRINGS = {
	EXTRA_STRING_TABLE(0)
};

CovetedMirrorGame::CovetedMirrorGame() : ComprehendGameV2() {
	_gameDataFile = "g0";

	// The Apple II release keeps its canned messages in twelve separate string
	// banks (MA-ML), the same way OO-Topos uses MA-ME. Each holds up to 64
	// strings, indexed via EXTRA_STRING_TABLE (0x8200 | n).
	addLetteredFiles(_stringFiles, "M", 'A', 'L');

	// Disk #1 carries the room (RA-RG) and object (OA-OG) pictures.
	addLetteredFiles(_locationGraphicFiles, "R", 'A', 'G');
	addLetteredFiles(_itemGraphicFiles, "O", 'A', 'G');

	_gameStrings = &CM_STRINGS;

	// The title is a Graphics Magician vector image in T0 (like OO-Topos).
	_titleGraphicFile = "t0";
}

// Show the title screen before the first room. The credits printed below the
// picture on the original Apple title screen are drawn as centred text, as in
// the other Comprehend titles.
void CovetedMirrorGame::beforeGame() {
	g_comprehend->drawPicture(TITLE_IMAGE);

	g_comprehend->setCentered(true);
	console_println("by Eagle Berns and Holly Thomason");
	console_println("Copyright 1986  Polarware/Penguin Software");
	g_comprehend->setCentered(false);
	g_comprehend->readChar();

	glk_window_clear(g_comprehend->_bottomWindow);
}

// The interpreter hard-codes a throne-room guard that no G0 bytecode expresses:
// in room 0x12 with flag 5 clear, any command is discarded and FUNC 000a runs,
// which prints the insolence line and moves the player to the prison (room 1).
// Nothing in G0 calls FUNC 000a. FUNC 0028 sets flag 5 once the player has
// progressed, after which the throne can be revisited.
void CovetedMirrorGame::handleAction(Sentence *sentence) {
	if (_currentRoom == 0x12 && !_flags[5]) {
		// The typed command is discarded entirely (the engine bypasses its
		// normal action dispatch), so run FUNC 000a with no sentence in scope.
		_specialOpcode = 0;
		eval_function(0x0a, nullptr);
		_functionNum = 0;
		eval_function(0, nullptr);
		handleSpecialOpcode();
		return;
	}

	ComprehendGameV2::handleAction(sentence);
}

// The each-turn daemon is not run here; see ComprehendGame::beforeTurn(). It
// drains one grain per turn, but the room picture, which redraws the hourglass,
// only repaints when the room or items change. So refresh the hourglass here,
// unless last turn's _updateFlags (consumed by update() shortly after) already
// queue a full repaint, which snaps the sand as the original does on a room
// change.
void CovetedMirrorGame::beforeTurn() {
	if (!(_updateFlags & (UPDATE_GRAPHICS | UPDATE_GRAPHICS_ITEMS)))
		g_comprehend->refreshCMHourglass();
}

// The string banks have no restart-prompt text: on THE END (win, or the 15th
// imprisonment) the original just freezes the machine. Keep the restart/quit
// choice, with a literal prompt.
Common::String CovetedMirrorGame::restartPrompt() {
	return "Press 'R' to play again, any other key to quit.";
}

// Remember a found mirror piece. The bytecode draws each piece once, with
// OPCODE_DRAW_OBJECT, when it is collected, and the original never repaints
// that band. This renderer composites the panel over every in-game picture,
// which would wipe them, so cmMirrorPieceMask() folds them into the panel
// snapshot (see pics.cpp).
void CovetedMirrorGame::recordItemPictureDrawn(int itemPic) {
	for (int i = 0; i < 4; i++)
		if (CM_PIECE_PICS[i] == itemPic) {
			_mirrorPieceMask |= (uint8)(1 << i);
			break;
		}
}

void CovetedMirrorGame::synchronizeSave(Common::Serializer &s) {
	ComprehendGameV2::synchronizeSave(s);
	// One trailing byte: the collected-mirror-piece set, graphics-only state
	// the VM data doesn't carry. The original's save snapshots the hi-res page
	// instead. This grows CM's on-disk save format by a byte.
	s.syncAsByte(_mirrorPieceMask);
}

// The wandering NPCs and objects (Starina, Pete Starnum, the witch outside the
// castle, the deaf mute, the catchable shadow and others) are spawned by engine
// code, not bytecode. On entering a new room the engine removes that room's
// wanderers and rolls one random byte to decide which, if any, reappears: rooms
// 0x2b, 0x34, 0x35, 0x38 and 0x3b choose among items with chance 0x40/0x100
// each; the rooms in this table show their item when random > threshold. GET
// SHADOW (FUNC 0ce) needs item 0x22 present, so without this the witch's brew
// cannot be completed.
static const uint8 kWanderRooms[9]  = {0x11, 0x28, 0x22, 0x31, 0x39, 0x49, 0x4d, 0x42, 0x0d};
static const uint8 kWanderItems[9]  = {0x0a, 0x13, 0x0e, 0x1d, 0x29, 0x36, 0x37, 0x31, 0x4d};
static const uint8 kWanderThresh[9] = {0x80, 0x4d, 0x4d, 0x4d, 0x4d, 0x4d, 0x80, 0x80, 0x80};

void CovetedMirrorGame::spawnWanderingNPCs() {
	if (_currentRoom == _lastSpawnRoom)
		return;
	_lastSpawnRoom = _currentRoom;

	uint8 r = g_comprehend->getRandomNumber(255);

	// Operands are 1-based item numbers, like the bytecode's.
	auto vanish = [&](uint8 num) { move_object(get_item(num - 1), ROOM_NOWHERE); };
	auto appear = [&](uint8 num) { move_object(get_item(num - 1), _currentRoom); };

	switch (_currentRoom) {
	case 0x2b:
		vanish(0x16); vanish(0x4f); vanish(0x18);
		if (r < 0x40) appear(0x16);
		else if (r < 0x80) appear(0x4f);
		else if (r < 0xc0) appear(0x18);
		break;
	case 0x34:
		vanish(0x1f); vanish(0x20); vanish(0x22);
		if (r < 0x40) appear(0x1f);
		else if (r < 0x80) appear(0x20);
		else if (r < 0xc0) appear(0x22);
		break;
	case 0x35:
		vanish(0x23); vanish(0x24);
		if (r < 0x40) appear(0x23);
		else if (r < 0x80) appear(0x24);
		break;
	case 0x38:
		vanish(0x27); vanish(0x46);
		if (r < 0x40) appear(0x27);
		else if (r < 0x80) appear(0x46);
		break;
	case 0x3b:
		vanish(0x2b); vanish(0x2c);
		if (r < 0x40) appear(0x2b);
		else if (r < 0x80) appear(0x2c);
		break;
	default:
		for (int i = 0; i < 9; i++)
			if (kWanderRooms[i] == _currentRoom) {
				vanish(kWanderItems[i]);
				if (r > kWanderThresh[i])
					appear(kWanderItems[i]);
				break;
			}
		break;
	}
}

// Relocate everything the player carries. Special opcodes 8 (the tavern
// pickpockets stash the inventory in room 0x1d) and 0x0c (Voar confiscates it
// to the treasure room 0x5e) need this.
void CovetedMirrorGame::moveCarriedItemsTo(uint8 room) {
	for (uint i = 0; i < _items.size(); i++)
		if (_items[i]._room == ROOM_INVENTORY)
			move_object(&_items[i], room);
}

// Special opcodes: 1 THE END (win), 2 game over (15 strikes), 6 save, 7
// restore, 8 tavern pickpockets, 9 colour-spell screen flash (graphics only,
// ignored here), 0x0c confiscation. The wandering-NPC re-roll runs after every
// one, because the original's handler falls through to it unconditionally.
void CovetedMirrorGame::handleSpecialOpcode() {
	switch (_specialOpcode) {
	case 0x01:
	case 0x02:
		// Win/lose: both end play; handle_restart offers RESTART/RESTORE/QUIT.
		game_restart();
		break;

	case 0x06:
		// Save game (same special opcode the other Comprehend games use).
		game_save();
		break;

	case 0x07:
		game_restore();
		break;

	case 0x08:
		moveCarriedItemsTo(0x1d);
		break;

	case 0x0c:
		moveCarriedItemsTo(0x5e);
		break;

	default:
		break;
	}
	_specialOpcode = 0;

	spawnWanderingNPCs();
}

} // namespace Comprehend
} // namespace Glk

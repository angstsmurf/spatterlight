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

namespace Glk {
namespace Comprehend {

// The banks contain no restart-prompt string (the original halts on THE END);
// handle_restart() below prints a literal instead, so this entry is unused.
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

// Show the title screen before the first room. CM ships its title as a T0
// vector image (rendered at offset 0x100, past the disk-protection stub) exactly
// like OO-Topos; without this override the base no-op beforeGame() left the game
// booting straight into the throne room with no title. The author/copyright
// credits printed below the picture on the original Apple title screen are drawn
// here as centred text, matching the other Comprehend titles (OO-Topos, etc.).
void CovetedMirrorGame::beforeGame() {
	g_comprehend->drawPicture(TITLE_IMAGE);

	g_comprehend->setCentered(true);
	console_println("by Eagle Berns and Holly Thomason");
	console_println("Copyright 1986  Polarware/Penguin Software");
	g_comprehend->setCentered(false);
	g_comprehend->readChar();

	glk_window_clear(g_comprehend->_bottomWindow);
}

// The Coveted Mirror's interpreter hard-codes a throne-room guard that no G0
// bytecode can express: while the player is in the throne room (room 0x12) with
// flag 5 clear, any command is discarded and King Voar imprisons the player.
// This is why nothing in G0 calls FUNC 000a -- it is reachable only from the
// engine. Reverse-engineered from the Apple II interpreter (cm_ram_raw $4092):
//
//     LDA $5a3d        ; current room
//     CMP #$12         ; throne?
//     BNE normal
//     LDA #$05         ; flag 5
//     JSR test_flag
//     BNE normal       ; flag 5 set -> ordinary play
//     LDA #$0a         ; else force FUNC 000a (imprisonment)...
//     STA func_lo
//     JMP run          ; ...bypassing the normal action dispatch entirely
//
// FUNC 000a prints the insolence line and moves the player to the prison
// (room 1); flag 5 is later set by FUNC 0028 once the player has progressed,
// after which the throne can be revisited freely. The flag is read live, so we
// mirror it here against the same _flags[5] the bytecode maintains.
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

// The Coveted Mirror's main command loop (RE'd from the live Apple II RAM dump at
// cm_main_command_loop $404c) runs the each-turn function chain -- function 0,
// the daemon -- exactly ONCE per turn, inside the dispatch after the matched
// action (cm_vm_dispatch_thunk $4f80 evaluates the matched function, then
// function 0). There is no daemon pass before the parser; the pre-prompt work in
// the original is only the room/item describe (FUN_4dc1) and graphics.
//
// The base ComprehendGame::beforeTurn() runs function 0 a second time before the
// prompt, which doubles every per-turn daemon effect. For most of that chain it
// is invisible, but the hourglass is decremented inside it (variable 0x11),
// so the double pass made the sand drain at 2/turn: the jailer caught the player
// after ~37 turns instead of the authentic ~74 (start value 74, -1/turn). That
// halved budget is why a faithful walkthrough's bribe cadence didn't fit.
//
// Override to suppress the extra pre-prompt daemon pass so function 0 runs once
// per turn, as the original does. (Scoped to CM: the other games override
// beforeTurn with their own per-turn logic and their loops aren't RE-verified
// here.)
//
// The daemon (run later, in the dispatch) drains one grain of sand per turn, but
// the room picture -- which is what redraws the hourglass -- only repaints when
// the room or its items change. So on a turn where the player stays put nothing
// redrew the draining sand, and the freed-grain fall was never armed. Give the
// hourglass its own per-turn refresh here, mirroring the original's per-turn
// step. Skip it when a full room repaint is already queued for this turn (set by
// last turn's move/item change): that path draws the hourglass itself and snaps,
// as the original does on a room change. _updateFlags still holds last turn's
// flags here -- update() consumes them moments later.
void CovetedMirrorGame::beforeTurn() {
	if (!(_updateFlags & (UPDATE_GRAPHICS | UPDATE_GRAPHICS_ITEMS)))
		g_comprehend->refreshCMHourglass();
}

// The string banks have no restart-prompt text: on THE END (win, or the 15th
// imprisonment) the original interpreter just freezes the machine. Keep the
// terp's restart/quit choice, but supply the prompt as a literal instead of
// printing a bogus bank string.
bool CovetedMirrorGame::handle_restart() {
	console_println("Press 'R' to play again, any other key to quit.");
	_ended = false;

	if (tolower(console_get_key()) == 'r') {
		loadGame();
		g_comprehend->clearUndo();
		_updateFlags = UPDATE_ALL;
		return true;
	}
	g_comprehend->quitGame();
	return false;
}

// The Coveted Mirror's wandering NPCs and objects -- Starina, Pete Starnum, the
// witch outside the castle, the deaf mute, the catchable shadow on the lane by
// Sue Sew-&-Tuck's, and friends -- are spawned by ENGINE code, not bytecode.
// RE'd from the live Apple II RAM dump (inside cm_handle_special_opcode, the
// block at $4150-$4283): each turn, if the player has entered a new room (last
// spawn room is kept at $4037), the engine removes that room's wanderers and
// re-rolls one random byte ($5b69) to decide which (if any) reappears:
//
//   room 0x2b: items 0x16/0x4f/0x18, each with chance 0x40/0x100; else none
//   room 0x34: items 0x1f/0x20/0x22 likewise (0x22 = the catchable shadow)
//   room 0x35: items 0x23/0x24
//   room 0x38: items 0x27/0x46
//   room 0x3b: items 0x2b/0x2c
//   else, a 9-entry room table ($4254: rooms, $425d: 1-based items, $4266:
//   thresholds): the item appears when random > threshold.
//
// This is why walkthroughs say "if Starina is not there, just go N,S,N,S until
// she shows up": leaving and re-entering re-rolls the spawn. GET SHADOW is
// gated on item 0x22 being present (FUNC 0ce), so without this engine system
// the shadow never appears and the witch's brew cannot be completed.
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

// The original's FUN_426f: walk the whole item table and relocate everything
// the player carries. Used by special opcode 8 (the tavern pickpockets stash
// your inventory in room 0x1d) and 0x0c (Voar confiscates your booty to the
// treasure room 0x5e when you are caught -- see the magician's diary).
void CovetedMirrorGame::moveCarriedItemsTo(uint8 room) {
	for (uint i = 0; i < _items.size(); i++)
		if (_items[i]._room == ROOM_INVENTORY)
			move_object(&_items[i], room);
}

// Special opcodes, RE'd from cm_handle_special_opcode ($4140):
//   1: THE END (win; bytecode has just printed "Congratulations!!")
//   2: game over (15 strikes: "...THE GAME IS OVER.")
//   6: SAVE verb (the bytecode's save function ends in OPCODE_SPECIAL 6)
//   7: RESTORE verb
//   8: tavern pickpockets steal the inventory into room 0x1d
//   9: the colour-spell screen flash (graphics only)
//   c: confiscate carried items to the treasure room 0x5e
// The wandering-NPC re-roll runs every turn, special or not, because the
// original's handler falls through to it unconditionally.
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
		// Restore game.
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

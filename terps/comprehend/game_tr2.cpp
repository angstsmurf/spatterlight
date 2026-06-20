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
#include "game_data.h"
#include "game_tr2.h"
#include "pics.h"

namespace Glk {
namespace Comprehend {

enum RoomId {
	ROOM_CLAY_HUT = 7,
	ROOM_FIELD = 26
};

enum RoomFlag {
	ROOMFLAG_FOREST = 1 << 0,
	ROOMFLAG_WEREWOLF = 1 << 6,
	ROOMFLAG_VAMPIRE = 1 << 7
};

enum ItemId {
	ITEM_GOBLIN = 9,
	ITEM_SILVER_BULLET = 21,
	ITEM_BLACK_CAT = 23,
	ITEM_WEREWOLF = 33,
	ITEM_VAMPIRE = 38
};

struct TransylvaniaMonster {
	uint8 _object;
	uint8 _deadFlag;
	uint _roomAllowFlag;
	uint _minTurnsBefore;
	uint _randomness;
};

const TransylvaniaMonster TransylvaniaGame2::WEREWOLF = {
	ITEM_WEREWOLF, 7, ROOMFLAG_WEREWOLF, 10, 190
};

const TransylvaniaMonster TransylvaniaGame2::VAMPIRE = {
	ITEM_VAMPIRE, 5, ROOMFLAG_VAMPIRE, 0, 200
};

static const GameStrings TR_STRINGS = {
	EXTRA_STRING_TABLE(0x8a)
};

TransylvaniaGame2::TransylvaniaGame2() : ComprehendGameV2(),
		_miceReleased(false), _restartImmediate(false) {
	_gameDataFile = "g0";

	_locationGraphicFiles.push_back("RA");
	_locationGraphicFiles.push_back("RB");
	_locationGraphicFiles.push_back("RC");
	_itemGraphicFiles.push_back("OA");
	_itemGraphicFiles.push_back("OB");
	_itemGraphicFiles.push_back("OC");

	_titleGraphicFile = "t0";

	_gameStrings = &TR_STRINGS;

	// Like Oo-Topos, the DOS Transylvania v2 stores its extra (_strings2) string
	// bank inside novel.exe rather than in separate files. It is five standard
	// structured segments (4-byte header + 64-entry index + data), giving the
	// 5 x 64 = 320 strings the bytecode addresses through table 0x82 (e.g. the
	// WAIT response 0x825d, OPEN COFFIN 0x824c and the restart prompt 0x828a).
	// Without this the engine printed BAD_STRING(82xx) for all of them.
	if (!Common::DiskImageFS::active()) {
		Common::File f;
		if (f.open("novel.exe")) {
			Common::String md5 = Common::computeStreamMD5AsString(f, 1024);
			f.close();

			if (md5 == "e708b5f744e1f9b59354722ab55added") {
				static const uint32 segHeaders[5] = {
					0x1636c, 0x1726c, 0x17c6c, 0x1846c, 0x18d6c
				};
				for (int i = 0; i < 5; i++)
					_stringFiles.push_back(
						StringFile::structuredSegment("novel.exe", segHeaders[i]));
			} else {
				warning("Unrecognised Transylvania novel.exe (md5 head %s); "
				        "extra strings unavailable", md5.c_str());
			}
		}
	}
}

bool TransylvaniaGame2::updateMonster(const TransylvaniaMonster *monsterInfo) {
	Item *monster;
	Room *room;
	uint16 turn_count;

	room = &_rooms[_currentRoom];
	if (!(room->_flags & monsterInfo->_roomAllowFlag))
		return false;

	turn_count = _variables[VAR_TURN_COUNT];
	monster = get_item(monsterInfo->_object);

	if (monster->_room == _currentRoom) {
		// The monster is in the current room - leave it there
		return true;
	}

	if (!_flags[monsterInfo->_deadFlag] &&
	        turn_count > monsterInfo->_minTurnsBefore) {
		/*
		 * The monster is alive and allowed to move to the current
		 * room. Randomly decide whether on not to. If not, move
		 * it back to limbo.
		 */
		if (getRandomNumber(255) > monsterInfo->_randomness) {
			move_object(monster, _currentRoom);
			_variables[15] = turn_count + 1;
		} else {
			move_object(monster, ROOM_NOWHERE);
		}
	}

	return true;
}

bool TransylvaniaGame2::isMonsterInRoom(const TransylvaniaMonster *monsterInfo) {
	Item *monster = get_item(monsterInfo->_object);
	return monster->_room == _currentRoom;
}

int TransylvaniaGame2::roomIsSpecial(uint room_index, uint *roomDescString) {
	Room *room = &_rooms[room_index];

	if (room_index == 0x28) {
		if (roomDescString)
			*roomDescString = room->_stringDesc;
		return ROOM_IS_DARK;
	}

	return ROOM_IS_NORMAL;
}

void TransylvaniaGame2::beforeTurn() {
	Room *room;

	if (!isMonsterInRoom(&WEREWOLF) && !isMonsterInRoom(&VAMPIRE)) {
		if (_currentRoom == ROOM_CLAY_HUT) {
			Item *blackCat = get_item(ITEM_BLACK_CAT);
			if (blackCat->_room == _currentRoom && getRandomNumber(255) >= 128)
				console_println(_strings[109].c_str());
			goto done;

		} else if (_currentRoom == ROOM_FIELD) {
			Item *goblin = get_item(ITEM_GOBLIN);
			if (goblin->_room == _currentRoom)
				console_println(_strings[94 + getRandomNumber(3)].c_str());
			goto done;

		}
	}

	if (updateMonster(&WEREWOLF) || updateMonster(&VAMPIRE))
		goto done;

	room = &_rooms[_currentRoom];
	if ((room->_flags & ROOMFLAG_FOREST) && (_variables[VAR_TURN_COUNT] % 255) >= 4
			&& getRandomNumber(255) < 40) {
		int stringNum = _miceReleased ? 108 : 107;
		console_println(_strings[stringNum].c_str());

		// Until the mice are released, an eagle moves player to a random room
		if (!_miceReleased) {
			// Get new room to get moved to
			int roomNum = getRandomNumber(3) + 1;
			if (roomNum == _currentRoom)
				roomNum += 15;

			move_to(roomNum);

			// Make sure Werwolf and Vampire aren't present
			get_item(ITEM_WEREWOLF)->_room = 0xff;
			get_item(ITEM_VAMPIRE)->_room = 0xff;
		}
	}

done:
	// Do NOT run the each-turn daemon (function 0) here. Transylvania v2 shares
	// the V2 command loop, RE-verified against the DOS interpreter
	// (TransylvaniaPC/Novel.exe main loop FUN_1000_05a0, dispatch FUN_1000_0a90):
	// the dispatch evaluates the matched action and then function 0 -- the daemon
	// -- exactly ONCE per turn (FUN_0a90 sets the function index to 0 and calls the
	// evaluator a second time), and the room/graphics display (FUN_16e9) sits at
	// the top of the loop, before the parser. There is no daemon pass before the
	// parser. Calling ComprehendGameV2::beforeTurn() (-> ComprehendGame::beforeTurn()
	// -> eval_function(0)) ran the daemon a second time, doubling every per-turn
	// function-0 effect. The monster movement above is our native port of the
	// original's special-default routine (FUN_1000_0710) and runs once per turn on
	// its own; only the spurious second function-0 pass is removed here. (Same fix
	// as CovetedMirrorGame / OOToposGame::beforeTurn(); see git 845e9d5d.)
	;
}

void TransylvaniaGame2::synchronizeSave(Common::Serializer &s) {
	ComprehendGame::synchronizeSave(s);
	s.syncAsByte(_miceReleased);

	// On restore, make sure the player doesn't come back to a stuck werewolf or
	// vampire in the room (they are placed afresh each turn by beforeTurn). This
	// must only run when *loading*: synchronizeSave also drives the per-turn #undo
	// snapshot (Comprehend::pushUndo -> serializeGameState), and mutating the live
	// item rooms there would wipe the monster the moment it appears -- before the
	// player could ever LOOK at, shoot or be threatened by it.
	if (s.isLoading()) {
		get_item(ITEM_WEREWOLF)->_room = 0xff;
		get_item(ITEM_VAMPIRE)->_room = 0xff;
	}
}

void TransylvaniaGame2::handleSpecialOpcode() {
	switch (_specialOpcode) {
	case 1:
		// Mice have been released
		_miceReleased = true;
		break;

	case 2:
		// Gun is fired. Drop the bullet in a random room
		get_item(ITEM_SILVER_BULLET)->_room = getRandomNumber(7) + 1;
		_updateFlags |= UPDATE_GRAPHICS;
		break;

	case 3:
	case 4:
		// Game over - failure. The bytecode has already printed the cause
		// (e.g. "Too late! The furry fiend just had you for dinner...");
		// handle_restart() shows the "Press 'R'..." prompt and waits.
		_restartImmediate = false;
		game_restart();
		break;

	case 5:
		// Won the game
		g_comprehend->showGraphics();
		g_comprehend->drawLocationPicture(40);
		_restartImmediate = false;
		game_restart();
		break;

	case 6:
		game_save();
		break;

	case 7:
		game_restore();
		break;

	case 8:
		// The RESTART verb: the original reloads at once, with no prompt
		// and no keypress (verified against novel.exe in DOSBox)
		_restartImmediate = true;
		game_restart();
		break;

	case 9:
		// Show the Zin screen in response to doing
		// 'sing some enchanted evening' in his cabin.
		g_comprehend->showGraphics();
		g_comprehend->drawLocationPicture(41);
		console_get_key();
		_updateFlags |= UPDATE_GRAPHICS;
		break;

	default:
		break;
	}
}

bool TransylvaniaGame2::handle_restart() {
	_ended = false;

	// The RESTART verb reloads silently; a game over (win or death) shows the
	// restart prompt ("Press 'R' if you would like to restart the novel.",
	// _strings2 0x828a) and waits for the player.
	if (!_restartImmediate) {
		console_println(stringLookup(_gameStrings->game_restart).c_str());
		printUndoAfterDeathHint();

		int c = console_get_key();
		if (undoAfterDeath(c))
			return true;
		if (tolower(c) != 'r') {
			g_comprehend->quitGame();
			return false;
		}
	}

	loadGame();
	g_comprehend->clearUndo();  // can't undo across a restart
	_updateFlags = UPDATE_ALL;
	return true;
}

// Read the player's reply at a "> " prompt on the line below the question,
// matching the original's guest-register input rather than echoing the typed
// name straight after the question mark.
#define READ_LINE do { \
	g_comprehend->print("> "); \
	g_comprehend->readLine(buffer, sizeof(buffer)); \
	if (g_comprehend->shouldQuit()) return; \
	} while (strlen(buffer) == 0)

void TransylvaniaGame2::beforeGame() {
	char buffer[128];
	g_comprehend->setDisableSaves(true);

	// Draw the title
	g_comprehend->drawPicture(TITLE_IMAGE);

	// Print game information, centred to match the other Comprehend titles.
	g_comprehend->setCentered(true);
	console_println("Story and graphics by Antonio Antiochia.");
	console_println("IBM version by Jeffrey A. Jay. Copyright 1987  POLARWARE, Inc.");
	g_comprehend->setCentered(false);
	g_comprehend->readChar();

	// Welcome to Transylvania - sign your name
	console_println(_strings[0x20].c_str());
	READ_LINE;

	// The player's name is stored in word 0
	_replaceWords[0] = Common::String(buffer);

	// And your next of kin - This isn't stored by the game
	console_println(_strings[0x21].c_str());
	READ_LINE;

	g_comprehend->setDisableSaves(false);
}

} // namespace Comprehend
} // namespace Glk

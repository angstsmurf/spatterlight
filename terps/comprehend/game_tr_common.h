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

/* The game logic Transylvania v1 (a V1-engine game) and v2 (the same story on
 * the V2 engine) share: everything but which files they load. Each of
 * game_tr1.cpp and game_tr2.cpp includes this and instantiates
 * TransylvaniaCommon over its own engine class.
 */

#ifndef GLK_COMPREHEND_GAME_TR_COMMON_H
#define GLK_COMPREHEND_GAME_TR_COMMON_H

#include "comprehend.h"
#include "game_opcodes.h"
#include "pics.h"

namespace Glk {
namespace Comprehend {

struct TransylvaniaMonster {
	uint8 _object;
	uint8 _deadFlag;
	uint _roomAllowFlag;
	uint _minTurnsBefore;
	uint _randomness;
};

template <typename Base>
class TransylvaniaCommon : public Base {
protected:
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

	static const TransylvaniaMonster WEREWOLF;
	static const TransylvaniaMonster VAMPIRE;

	bool _miceReleased = false;
	// Whether the RESTART verb reloads without the restart prompt.
	const bool _silentRestartVerb;

	explicit TransylvaniaCommon(bool silentRestartVerb);

	bool updateMonster(const TransylvaniaMonster *monsterInfo);
	bool isMonsterInRoom(const TransylvaniaMonster *monsterInfo);

	// Members of the engine base class the shared code uses. The base is a
	// template parameter, so they have to be named here to be found.
	using Base::_currentRoom;
	using Base::_flags;
	using Base::_gameStrings;
	using Base::_replaceWords;
	using Base::_rooms;
	using Base::_specialOpcode;
	using Base::_strings;
	using Base::_updateFlags;
	using Base::_variables;
	using Base::console_get_key;
	using Base::console_println;
	using Base::game_restart;
	using Base::game_restore;
	using Base::game_save;
	using Base::getRandomNumber;
	using Base::get_item;
	using Base::move_object;
	using Base::move_to;

public:
	void beforeGame() override;
	void beforeTurn() override;
	void synchronizeSave(Common::Serializer &s) override;
	int roomIsSpecial(uint room_index, uint *roomDescString) override;
	void handleSpecialOpcode() override;
};

template <typename Base>
const TransylvaniaMonster TransylvaniaCommon<Base>::WEREWOLF = {
	ITEM_WEREWOLF, 7, ROOMFLAG_WEREWOLF, 10, 190
};

template <typename Base>
const TransylvaniaMonster TransylvaniaCommon<Base>::VAMPIRE = {
	ITEM_VAMPIRE, 5, ROOMFLAG_VAMPIRE, 0, 200
};

template <typename Base>
TransylvaniaCommon<Base>::TransylvaniaCommon(bool silentRestartVerb) :
		Base(), _silentRestartVerb(silentRestartVerb) {
	static const GameStrings TR_STRINGS = {
		EXTRA_STRING_TABLE(0x8a)
	};
	_gameStrings = &TR_STRINGS;
}

template <typename Base>
bool TransylvaniaCommon<Base>::updateMonster(const TransylvaniaMonster *monsterInfo) {
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

template <typename Base>
bool TransylvaniaCommon<Base>::isMonsterInRoom(const TransylvaniaMonster *monsterInfo) {
	Item *monster = get_item(monsterInfo->_object);
	return monster->_room == _currentRoom;
}

template <typename Base>
int TransylvaniaCommon<Base>::roomIsSpecial(uint room_index, uint *roomDescString) {
	Room *room = &_rooms[room_index];

	if (room_index == 0x28) {
		if (roomDescString)
			*roomDescString = room->_stringDesc;
		return ROOM_IS_DARK;
	}

	return ROOM_IS_NORMAL;
}

template <typename Base>
void TransylvaniaCommon<Base>::beforeTurn() {
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
	// The monster movement above is our native port of the original's
	// special-default routine (FUN_1000_0710 in the DOS v2 Novel.exe). The
	// each-turn daemon is not run here; see ComprehendGame::beforeTurn().
	;
}

template <typename Base>
void TransylvaniaCommon<Base>::synchronizeSave(Common::Serializer &s) {
	this->ComprehendGame::synchronizeSave(s);
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

template <typename Base>
void TransylvaniaCommon<Base>::handleSpecialOpcode() {
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
		game_restart();
		break;

	case 5:
		// Won the game
		g_comprehend->showGraphics();
		g_comprehend->drawLocationPicture(40);
		game_restart();
		break;

	case 6:
		game_save();
		break;

	case 7:
		game_restore();
		break;

	case 8:
		// The RESTART verb. In v2 the original reloads at once, with no
		// prompt and no keypress (verified against novel.exe in DOSBox).
		game_restart(_silentRestartVerb);
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

// Read the player's reply at a "> " prompt on the line below the question,
// matching the original's guest-register input rather than echoing the typed
// name straight after the question mark.
#define READ_LINE do { \
	g_comprehend->print("> "); \
	g_comprehend->readLine(buffer, sizeof(buffer)); \
	if (g_comprehend->shouldQuit()) return; \
	} while (strlen(buffer) == 0)

template <typename Base>
void TransylvaniaCommon<Base>::beforeGame() {
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

#undef READ_LINE

} // namespace Comprehend
} // namespace Glk

#endif

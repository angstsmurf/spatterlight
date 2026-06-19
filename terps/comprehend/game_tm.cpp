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
	// Item art is numbered against the full OA..OF sequence ((graphic-1) / 16
	// picks the file), but only OA, OB, OE and OF ship -- OC and OD are absent.
	// They must still occupy slots 2 and 3 so OE/OF stay at slots 4/5, otherwise
	// part 2's item pictures (e.g. the crow's-nest parrot, graphic 70 -> OE) run
	// off the end of the array. Pics::load() substitutes an empty placeholder for
	// the missing OC/OD.
	_itemGraphicFiles.push_back("OA");
	_itemGraphicFiles.push_back("OB");
	_itemGraphicFiles.push_back("OC");
	_itemGraphicFiles.push_back("OD");
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
	// On the Apple II the strings live in standard string-table files instead
	// of being embedded in novel.exe. Part 1 uses 15 banks MA-MO (MA-MN on the
	// boot disk, MO on the Empire disk); part 2 (the desert, on the Lands Beyond
	// disk) uses its own 9 banks MA,MB,MQ-MW -- the Apple equivalent of the DOS
	// novel.exe's second string segment. Banks 0-7 fill _strings, 8+ _strings2,
	// matching the layout the bytecode references. The Lands Beyond disk carries
	// its own MA/MB, and is the active disk in part 2, so those resolve to the
	// part-2 versions.
	static const char *const BANKS_PART1[] = {
		"MA", "MB", "MC", "MD", "ME", "MF", "MG", "MH",
		"MI", "MJ", "MK", "ML", "MM", "MN", "MO"
	};
	const char *const *BANKS = BANKS_PART1;
	const int bankCount = (int)(sizeof(BANKS_PART1) / sizeof(BANKS_PART1[0]));

	_strings.clear();
	_strings2.clear();

	if (_inPart2) {
		loadStringsApplePart2();
		return;
	}

	for (int bank = 0; bank < bankCount; ++bank) {
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

void TalismanGame::loadStringsApplePart2() {
	// Part 2 (the desert) keeps its strings in 9 bank files on the Lands Beyond
	// disk, but the bytecode does NOT reference them as a contiguous run. As in
	// the DOS novel.exe's second string segment, the room/scene descriptions sit
	// in the first table (MA,MB -> _strings banks 0,1, reached via string tables
	// 0x80/0x81), while every message bank (MQ-MW) is addressed through the high
	// _strings2 tables 0x84/0x85 -- i.e. _strings2 offset 0x200 onwards. So MQ
	// must land at _strings2[0x200], MR at [0x240], and so on, leaving the lower
	// _strings/_strings2 slots empty. (Verified against the live game: the
	// per-turn ocean messages decode as table-0x85 references, e.g. 0x8563 ->
	// _strings2[0x363] == MV string 35, "The ship slices through the waves".)
	struct Placement { const char *file; bool strings2; int offset; };
	static const Placement P2[] = {
		{ "MA", false, 0x000 }, { "MB", false, 0x040 },
		{ "MQ", true,  0x200 }, { "MR", true,  0x240 }, { "MS", true,  0x280 },
		{ "MT", true,  0x2c0 }, { "MU", true,  0x300 }, { "MV", true,  0x340 },
		{ "MW", true,  0x380 }
	};

	// Size the tables to cover the highest offset any string table can address
	// (0x80/0x81 -> _strings up to 0x200; 0x82-0x85 -> _strings2 up to 0x400).
	_strings.assign(0x200, "");
	_strings2.assign(0x400, "");

	for (const Placement &p : P2) {
		StringTable &table = p.strings2 ? _strings2 : _strings;

		FileBuffer fb(p.file);
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
				table[p.offset + i] = parseString(&fb);
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
	if (!f.open("novel.exe") && !f.open("novel1.exe"))
		error("novel.exe is a required file");

	Common::String md5 = Common::computeStreamMD5AsString(f, 1024);
	// 15cd75f9... = NOVEL1.EXE, byte-identical to the 2e18c88c NOVEL.EXE from 0x400
	// on (only one header byte differs), so the string offsets below are the same.
	if (md5 != "0e7f002971acdb055f439020363512ce" && md5 != "2e18c88ce352ebea3e14177703a0485f" &&
		md5 != "15cd75f98788071aee2af1f63893f613")
		warning("Unrecognised novel.exe encountered; strings may be incorrect");

	// Part 1 reads novel.exe's first string segment as a contiguous run of 15
	// banks (banks 0-7 -> _strings, 8+ -> _strings2). Part 2 (the desert) reads
	// the second segment, whose 9 banks are NOT contiguous: the bytecode
	// addresses them in the same gapped layout as the Apple Lands Beyond banks
	// (see loadStringsApplePart2) -- banks 0,1 -> _strings 0x000,0x040 and banks
	// 2-8 -> _strings2 0x200,0x240,...,0x380, leaving the other slots empty.
	const int segment = _inPart2 ? STRINGS_SEGMENT2 : STRINGS_SEGMENT1;

	if (_inPart2) {
		_strings.assign(0x200, "");
		_strings2.assign(0x400, "");
	}

	f.seek(segment);
	for (int bank = 0; bank < BANKS_COUNT; ++bank)
		bankOffsets[bank] = f.readUint16LE();

	// Iterate through the banks loading the strings
	for (int bank = 0; bank < BANKS_COUNT; ++bank) {
		if (!bankOffsets[bank])
			continue;

		// Where this bank's strings land. Part 1 appends; part 2 places them at
		// the fixed gapped offset the bytecode references.
		StringTable &table = _inPart2 ? (bank < 2 ? _strings : _strings2)
		                              : (bank < 8 ? _strings : _strings2);
		const int base = _inPart2 ? (bank < 2 ? bank * 0x40 : 0x200 + (bank - 2) * 0x40)
		                          : -1;

		f.seek(segment + bankOffsets[bank]);
		for (int strNum = 0; strNum <= STRINGS_PER_BANK; ++strNum)
			stringOffsets[strNum] = f.readUint16LE();

		for (int strNum = 0; strNum < STRINGS_PER_BANK; ++strNum) {
			int size = stringOffsets[strNum + 1] - stringOffsets[strNum];
			if (size < 0)
				size = 0xfff;

			f.seek(segment + bankOffsets[bank] + stringOffsets[strNum]);
			FileBuffer fb(&f, size);
			Common::String str = parseString(&fb);

			if (_inPart2)
				table[base + strNum] = str;
			else
				table.push_back(str);
		}
	}
}

void TalismanGame::enterPart2() {
	// Part 2 is a separate game database on the Lands Beyond disk. Reloading it
	// (clearGame) wipes the rooms, items, functions and -- crucially -- the
	// flags and variables, so snapshot the player's progress and restore it
	// afterwards.
	bool savedFlags[MAX_FLAGS];
	uint16 savedVars[MAX_VARIABLES];
	Common::copy(&_flags[0], &_flags[MAX_FLAGS], savedFlags);
	Common::copy(&_variables[0], &_variables[MAX_VARIABLES], savedVars);

	// Abu walks ashore still carrying everything he held at the end of part 1
	// (verified against the Apple II original: the desert inventory is the
	// scimitar, coins, flask, torch, jade rod, flint, rope, shield and book).
	// Reloading the part-2 database resets every item to its static part-2
	// placement, which would strand the carried objects -- in particular the
	// torch (part-2 room 22) and flint (room 21), which are mandatory to enter
	// the statue maze and never appear anywhere reachable in the desert. Part 1
	// and part 2 share item indices for these objects, so the player's inventory
	// carries over faithfully by index: snapshot which items are held now, then
	// re-place them in the inventory after the part-2 item table is loaded.
	Common::Array<uint> carried;
	for (uint i = 0; i < _items.size(); ++i)
		if (_items[i]._room == ROOM_INVENTORY)
			carried.push_back(i);

	_inPart2 = true;
	if (Common::DiskImageFS::active()) {
		// Apple II: the part-2 database is the "g0" on the Lands Beyond disk;
		// switch the active disk so loadGame() resolves "G0" to it.
		Common::DiskImageFS::selectDiskByGameMagic(0x94c6);
	} else {
		// DOS: part 1 and part 2 ship as two separate data files in the same
		// directory -- G0 (magic 0xa429) and H0 (magic 0x94c6). Point the loader
		// at H0 for the desert half. (NOVEL.EXE's special-opcode 16 does the same
		// file switch; the part-2 strings come from novel.exe's second segment.)
		_gameDataFile = "H0";
	}
	loadGame();      // reloads part-2 rooms/items/functions/dictionary/...
	loadStrings();   // reloads strings from the part-2 banks (MA,MB,MQ-MW)

	Common::copy(&savedFlags[0], &savedFlags[MAX_FLAGS], _flags);
	Common::copy(&savedVars[0], &savedVars[MAX_VARIABLES], _variables);

	for (uint i = 0; i < carried.size(); ++i)
		if (carried[i] < _items.size())
			_items[carried[i]]._room = ROOM_INVENTORY;

	// Drop the player onto the ship's deck (part-2 room 51), the same room
	// NOVEL.EXE's special-opcode 16 selects (0x34b9 = 0x33).
	_currentRoom = 51;

	// Run the part-2 initialiser (func 44 -> func 45 -> func 160): it sets the
	// endgame room graphics, places the genie's bronze lamp in room 39, sets the
	// item long-descriptions and arms the moving-wall maze (flag 79). NOVEL.EXE's
	// special-opcode 16 runs this; it has no in-bytecode caller, so without this
	// the lamp never appears for the genie/wizard finale.
	eval_function(44, nullptr);

	if (getenv("TM_DUMP")) {
		FILE *f = fopen("/tmp/tm_part2_dump.txt", "w");
		static const char *dn[] = {"N","S","E","W","U","D","IN","OUT"};
		fprintf(f, "=== ROOMS (%u) ===\n", (uint)_rooms.size());
		for (uint i = 0; i < _rooms.size(); ++i) {
			const Room &r = _rooms[i];
			fprintf(f, "room %u flags=0x%02x gfx=%u exits[", i, r._flags, r._graphic);
			for (int d = 0; d < NR_DIRECTIONS; ++d)
				if (r._direction[d]) fprintf(f, "%s=%u ", dn[d], r._direction[d]);
			fprintf(f, "] desc=\"%s\"\n", stringLookup(r._stringDesc).c_str());
		}
		fprintf(f, "\n=== ITEMS (%u) ===\n", (uint)_items.size());
		for (uint i = 0; i < _items.size(); ++i) {
			const Item &it = _items[i];
			fprintf(f, "item %u room=%u word=%u flags=0x%02x desc=\"%s\"\n",
				i, it._room, it._word, it._flags, stringLookup(it._stringDesc).c_str());
		}
		fprintf(f, "\n=== FUNCTIONS (%u) ===\n", (uint)_functions.size());
		for (uint fi = 0; fi < _functions.size(); ++fi) {
			const Function &fn = _functions[fi];
			fprintf(f, "func %u:\n", fi);
			for (uint ii = 0; ii < fn.size(); ++ii) {
				const Instruction &in = fn[ii];
				uint8 norm = getScriptOpcode(&in);
				fprintf(f, "  %s op=0x%02x norm=0x%02x", in._isCommand ? "CMD " : "TEST", in._opcode, norm);
				for (uint oi = 0; oi < in._nr_operands; ++oi)
					fprintf(f, " %u", in._operand[oi]);
				fprintf(f, "\n");
			}
		}
		fprintf(f, "\n=== STRINGS table0 (%u) ===\n", (uint)_strings.size());
		for (uint i = 0; i < _strings.size(); ++i)
			fprintf(f, "[s0:%u] %s\n", i, _strings[i].c_str());
		fprintf(f, "\n=== STRINGS2 (%u) ===\n", (uint)_strings2.size());
		for (uint i = 0; i < _strings2.size(); ++i)
			fprintf(f, "[s2:%u] %s\n", i, _strings2[i].c_str());
		fprintf(f, "\n=== DICTIONARY (%u) ===\n", (uint)_words.size());
		for (uint i = 0; i < _words.size(); ++i)
			fprintf(f, "word idx=%u type=%u \"%s\"\n", _words[i]._index, _words[i]._type, _words[i]._word);
		fprintf(f, "\n=== ACTIONS ===\n");
		for (uint t = 0; t < _actions.size(); ++t) {
			for (uint a = 0; a < _actions[t].size(); ++a) {
				const Action &ac = _actions[t][a];
				fprintf(f, "table %u action %u func=%u words[", t, a, ac._function);
				for (uint w = 0; w < ac._nr_words; ++w) {
					const char *txt = "?";
					for (uint k = 0; k < _words.size(); ++k)
						if (_words[k]._index == ac._words[w]) { txt = _words[k]._word; break; }
					fprintf(f, "%u(%s) ", ac._words[w], txt);
				}
				fprintf(f, "]\n");
			}
		}
		fclose(f);
		fprintf(stderr, "[TM_DUMP] wrote /tmp/tm_part2_dump.txt\n");
	}
}

void TalismanGame::prepareRestore(const std::vector<byte> &payload) {
	// Talisman is two games in one: part 2 (the desert) is a separate database
	// with its own room/item/function tables, dictionary, '@'-replacement words
	// AND string banks. A saved game stores only the dynamic state (rooms,
	// items, flags, variables); the static tables are reloaded from disk. So a
	// part-2 save restored while part 1's database is loaded resolves every
	// part-2 index against part-1's data -- the desert beach (room 55) prints
	// part 1's "King's audience chamber", and its "@ern horizon" direction word
	// indexes part 1's replacement table (printing garbage like "She's not").
	//
	// _inPart2 is not itself persisted (and a part-2 save is the same size as a
	// part-1 one, so it can't be told apart by length), so detect a desert save
	// by flag 79: the moving-wall maze, armed by part 2's func 44. It is clear
	// at the end of part 1 and set on entry to part 2, so a restored save with
	// flag 79 set can only be from the desert. Swap in the whole part-2 database
	// before the saved state is deserialized over it.
	//
	// (One cosmetic limitation remains: the current '@'-replacement word index
	// is live, per-turn state that the save format doesn't store, so a desert
	// room whose description ends in "@ern horizon" shows a stale word on the
	// very first frame after a restore until the next turn re-derives it.)
	//
	// Payload layout (see ComprehendGame::synchronizeSave): currentRoom (2) +
	// variables (MAX_VARIABLES * 2) + flags (MAX_FLAGS), so flag N is at
	// 2 + MAX_VARIABLES * 2 + N.
	const uint flag79Off = 2 + MAX_VARIABLES * 2 + 79;
	if (_inPart2 || payload.size() <= flag79Off || !payload[flag79Off])
		return;

	_inPart2 = true;
	if (Common::DiskImageFS::active())
		Common::DiskImageFS::selectDiskByGameMagic(0x94c6);
	else
		_gameDataFile = "H0";
	loadGame();      // part-2 rooms/items/functions/dictionary/replace-words
	loadStrings();   // part-2 string banks
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

	if (getenv("TM_TRACE")) {
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
		// [0x9591] = [0x9592], where 0x9592 mirrors the room the player started
		// this turn in, before any move this turn). Paired with special 4, this
		// lets the game whisk the player away for a scripted scene and then
		// return them to where they were. It must be the *pre-move* room: the
		// magic lamp moves the player into its interior and only then fires
		// special 3, so saving the live room (or _currentRoomCopy, which move_to
		// overwrites mid-turn) would trap the player inside the lamp forever.
		_savedRoom = _roomBeforeTurn;
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
		// Boat departure: the player sails out of the Persian Empire and the game
		// crosses into part 2 (the desert). NOVEL.EXE case 16 (FUN_1000_06f0)
		// switches to graphics, sets the room to 0x33 (51), runs a function, then
		// falls through into a fresh copy of the main command loop -- the part-2
		// game loop. On the Apple II that loop runs against a wholly separate game
		// database loaded from the Lands Beyond disk, so we swap it in here.
		//
		// The departure daemon (func 20, offsets 23-25) emits this every turn via
		//   TEST flag45 -> PRINT(103,130) "splash on board" -> SPECIAL 16
		// and never clears flag 45 (unlike the intro-reprieve block in the same
		// function, offsets 5-17, which clears its own gate flag 116). Left as-is
		// the departure would re-narrate forever; flag 45 is tested *only* by this
		// block, so clearing it makes the scene self-limiting like the reprieve.
		//
		// enterPart2() swaps in the desert database: the Lands Beyond disk's g0 on
		// the Apple II, or the H0 data file plus novel.exe's second string segment
		// on DOS (see enterPart2()/loadStrings()).
		if (!_inPart2) {
			if (!g_comprehend->isGraphicsEnabled())
				g_comprehend->setGraphicsMode(true);
			enterPart2();
		}
		_flags[45] = false;
		// enterPart2() sets _currentRoom directly (not via move_to), so nothing
		// has armed the update flags for the new location. Without this the first
		// part-2 turn keeps the old room's status-window description and picture
		// until the player next moves. Repaint the ship's deck now, the way
		// NOVEL.EXE's case 16 falls into a fresh main loop that redraws on entry.
		_updateFlags |= UPDATE_ALL;
		update();
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
		// NPC-conversation trigger (emitted by the abu/executioner/clerk verb
		// handlers, funcs 37/114/261). NOVEL.EXE case 0x14 (FUN_1000_06f0) does
		// two things: it always saves the current verb as a forced verb for the
		// next parse (0x9593 = 0x35db), and -- only when that parse was the last
		// token of the line ([0x35e7] == 2) -- it clears that forced verb and
		// runs function 16 (the dialogue printer; via [0x34b4] = 16 +
		// FUN_1000_1b20). A bare NPC command ends the line, so [0x35e7] == 2 and
		// the function-16 path is what is observed in play; we take it directly.
		// The forced-verb half only matters when more input follows the NPC verb
		// on the same line (chained commands), which this fork's NOUNSTATE_QUERY
		// verb re-use already covers, so it is intentionally not duplicated here.
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

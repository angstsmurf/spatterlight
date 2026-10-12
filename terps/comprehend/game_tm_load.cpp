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

// Talisman's database loading: the string banks (novel.exe segments on DOS,
// MA.. bank files on the Apple II) and the swap to the part-2 (desert) database.
// The game logic proper is in game_tm.cpp.

namespace Glk {
namespace Comprehend {

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

#ifdef COMPREHEND_DEBUG
// Debug aid (build with -DCOMPREHEND_DEBUG, run with TM_DUMP=<file>): write the
// freshly loaded part-2 database -- rooms, items, functions, strings,
// dictionary and action tables -- as text.
void TalismanGame::dumpDatabase(const char *path) {
	FILE *f = fopen(path, "w");
	if (!f) return;
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
	fprintf(stderr, "[TM_DUMP] wrote %s\n", path);
}
#endif

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

#ifdef COMPREHEND_DEBUG
	if (const char *dump = getenv("TM_DUMP"))
		dumpDatabase(dump);
#endif
}

} // namespace Comprehend
} // namespace Glk

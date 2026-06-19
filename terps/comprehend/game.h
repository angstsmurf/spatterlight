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

#ifndef GLK_COMPREHEND_GAME_H
#define GLK_COMPREHEND_GAME_H

#include "game_data.h"
#include "comprehend_compat.h"

namespace Glk {
namespace Comprehend {

#define ROOM_IS_NORMAL 0
#define ROOM_IS_DARK 1
#define ROOM_IS_TOO_BRIGHT 2
#define INPUT_LINE_SIZE 1024

enum NounState { NOUNSTATE_STANDARD = 0, NOUNSTATE_QUERY = 1, NOUNSTATE_INITIAL = 2 };

enum RedoLine { REDO_NONE, REDO_PROMPT, REDO_TURN };

struct GameStrings;
struct Sentence;

struct Sentence {
	Word _words[20];
	size_t _nr_words;
	byte _formattedWords[6];
	byte _specialOpcodeVal2;
	// Gender/number nibble (_type & 0xf0) of the first noun, captured by
	// format().  Copied into GameData::_wordFlags so OPCODE_SET_STRING_REPLACEMENT3
	// can pick the article-agreeing @-replacement word (matches the Apple II
	// interpreter's cm_current_noun_flags / $5b6a, set by cm_format_sentence_words).
	byte _wordFlags;

	Sentence() {
		clear();
	}

	bool empty() const {
		return !_formattedWords[0];
	}

	/**
	 * Clears the sentence
	 */
	void clear();

	/**
	 * Copies from another sentence to this one
	 */
	void copyFrom(const Sentence &src, bool copyNoun = true);

	/**
	 * Splits up the array of _words into a _formattedWords
	 * array, placing the words in appropriate noun, verb, etc.
	 * positions appropriately
	 */
	void format();
};

class ComprehendGame : public GameData {
protected:
	bool _ended;
	NounState _nounState;
	Sentence _sentence;
	char _inputLine[INPUT_LINE_SIZE];
	int _inputLineIndex;
	int _currentRoomCopy;
	// Room the player occupied at the start of the current turn, before any
	// move this turn. Unlike _currentRoomCopy (which move_to() updates so the
	// graphics path can draw the room just moved into), this is left alone by
	// move_to, so Talisman's special-opcode 3 can remember where the player
	// came from -- e.g. the tunnel the magic lamp whisks them out of.
	int _roomBeforeTurn;
	int _functionNum;
	int _specialOpcode;
	RedoLine _redoLine;
public:
	const GameStrings *_gameStrings;

private:
	void describe_objects_in_current_room();
	void eval_instruction(FunctionState *func_state,
		const Function &func, uint functionOffset,
		const Sentence *sentence);
	void skip_whitespace(const char **p);
	void skip_non_whitespace(const char **p);
	bool handle_sentence(Sentence *sentence);
	bool handle_sentence(uint tableNum, Sentence *sentence, Common::Array<byte> &words);
	Common::String expandLetterShortcut(char letter);
	void read_sentence(Sentence *sentence);
	void parse_sentence_word_pairs(Sentence *sentence);
	void read_input();
	void doBeforeTurn();

protected:
	void game_save();
	bool game_restore();
	void game_restart() {
		_ended = true;
	}
	virtual bool handle_restart();
	// Forced restart used by the #restart metacommand: reload from the start
	// without the in-game RESTART/RESTORE/QUIT prompt.
	virtual void restartGame();

	// Shared "U to undo" handling for a game-over / restart prompt. Call with the
	// key the player pressed at the prompt: if it is the undo key and there is a
	// turn to step back to, restores that turn, clears the ended state, refreshes
	// the display and returns true (the caller should resume play). Steps back a
	// single turn -- these games' deaths are immediate, unlike Talisman's delayed
	// countdowns (which undo two; see TalismanGame::deathMenu). Returns false
	// otherwise, leaving restart/quit to the caller.
	bool undoAfterDeath(int key);
	// Advertises the undo key on its own line, alongside the game's restart prompt.
	void printUndoAfterDeathHint();

	virtual void execute_opcode(const Instruction *instr, const Sentence *sentence,
		FunctionState *func_state) = 0;

	int console_get_key();
	void console_println(const char *text);
	// Expand '@' string-replacement markers (the current replacement word or
	// number) the same way console_println does, but return the result rather
	// than printing it -- used so the room-description status window shows the
	// substituted text instead of a literal '@'.
	Common::String expandReplacementWords(const Common::String &text);
	void move_object(Item *item, int new_room);

	/*
	 * V2 interpreters (e.g. Talisman) reserve variable 0 as the input-number
	 * register: read_sentence_format zeroes it each turn and a numeric token
	 * fills it, and the VAR_*1 opcodes compare against it. V1 interpreters
	 * instead use variable 0 as the persisted inventory weight. The two uses
	 * collide, so number-register handling (the per-turn reset and numeric
	 * token capture in read_sentence, and suppressing the move_object weight
	 * write) is gated on this predicate.
	 */
	virtual bool hasNumberRegister() const { return false; }

	/*
	 * Comprehend functions consist of test and command instructions (if the MSB
	 * of the opcode is set then it is a command). Functions are parsed by
	 * evaluating each test until a command instruction is encountered. If the
	 * overall result of the tests was true then the command instructions are
	 * executed until either a test instruction is found or the end of the function
	 * is reached. Otherwise the commands instructions are skipped over and the
	 * next test sequence (if there is one) is tried.
	 */
	void eval_function(uint functionNum, const Sentence *sentence);

	void parse_header(FileBuffer *fb) override {
		GameData::parse_header(fb);
	}

	Item *get_item_by_noun(byte noun);
	int get_item_id(byte noun);
	void weighInventory();
	size_t num_objects_in_room(int room);
	void doMovementVerb(uint verbNum);

public:
	ComprehendGame();
	virtual ~ComprehendGame();

	/**
	 * Called before the game starts
	 */
	virtual void beforeGame() {}

	/**
	 * Called just before the prompt for user input
	 */
	virtual void beforePrompt() {}

	/**
	 * Called after input has been entered.
	 */
	virtual void afterPrompt() {}

	/**
	 * Called before the start of a game turn
	 */
	virtual void beforeTurn();

	/**
	 * Called at the end of a game turn
	 */
	virtual void afterTurn() {}

	/**
	 * Called when an action function has been selected
	 */
	virtual void handleAction(Sentence *sentence);

	virtual int roomIsSpecial(uint room_index, uint *room_desc_string) {
		return ROOM_IS_NORMAL;
	}
	virtual void handleSpecialOpcode() {}

	virtual void synchronizeSave(Common::Serializer &s);

	// Hook run just before a saved game's payload is deserialized over the live
	// state, given the raw payload bytes. Lets a game swap in a different static
	// database first (Talisman uses it to load its part-2 desert database when
	// restoring a desert save while part 1 is loaded). Default: nothing to do.
	virtual void prepareRestore(const std::vector<byte> &payload) { (void)payload; }

	// Called for every OPCODE_DRAW_OBJECT, with the 0-based item-picture number
	// just drawn. The Coveted Mirror uses it to remember which mirror pieces have
	// been collected so its persistent right-hand panel can keep showing them
	// (the originals are drawn once and never repainted); other games ignore it.
	virtual void recordItemPictureDrawn(int /*itemPic*/) {}

	// Bitmask of The Coveted Mirror's collected mirror pieces, in panel bit order
	// (see CM_PIECE_PICS). 0 for every other game; the graphics layer composites
	// the set pieces into the panel snapshot.
	virtual uint8 cmMirrorPieceMask() const { return 0; }

	// True if the just-restored state is self-consistent enough to keep playing
	// (mainly: the current room is in range). Used to reject a corrupt restore
	// before the bad state crashes the renderer.
	bool saveStateLooksValid() const {
		return _currentRoom >= 1 && _currentRoom < _rooms.size();
	}

	virtual ScriptOpcode getScriptOpcode(const Instruction *instr) = 0;

	Common::String stringLookup(uint16 index);
	Common::String instrStringLookup(uint8 index, uint8 table);

	virtual void playGame();

	void move_to(uint8 room);
	Room *get_room(uint16 index);
	Item *get_item(uint16 index);
	void updateRoomDesc();
	void transcribeCurrentRoom();
	void update();
	void update_graphics();

	/**
	 * Gets a random number
	 */
	uint getRandomNumber(uint max) const;
};

} // namespace Comprehend
} // namespace Glk

#endif

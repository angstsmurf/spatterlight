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

/* Based on Archetype interpreter version 1.01.
 * Ported from ScummVM to Spatterlight: GlkAPI inheritance is replaced by
 * direct calls to the Glk C API in glk.h.
 */

#ifndef ARCHETYPE_ARCHETYPE
#define ARCHETYPE_ARCHETYPE

#include <string>

#include "archetype_compat.h"
#include "array.h"
#include "interpreter.h"
#include "semantic.h"
#include "statement.h"
#include "archetype_string.h"

extern "C" {
#include "glk.h"
#include "randomness.h"
}

namespace Glk {
namespace Archetype {

enum DebugFlag {
    DEBUG_BYTES = 1 << 10,
    DEBUG_MSGS  = 1 << 11,
    DEBUG_EXPR  = 1 << 12,
    DEBUG_STMT  = 1 << 13
};

class Archetype {
private:
    int _saveSlot;
    winid_t _mainWindow;
    String _lastOutputText;
    bool _shouldQuit;

    // Engine-level meta commands (transcript / undo / restart). The story's own
    // verb set (compiled into the .ACX) has none of these, so they are layered
    // on in readLine(): the player's input is intercepted before it ever
    // reaches the game's parser. undo/restart work by snapshotting the dynamic
    // game state (the same serialization used by save/restore) and, on
    // re-entry, replaying the story's START message with output silenced until
    // the snapshot is reinstated at the first prompt.
    enum ReentryMode { RM_NONE, RM_UNDO, RM_RESTART };
    ReentryMode _reentry;          // pending re-entry into a fresh START run
    bool _replaySilencing;         // gag output while replaying START to the loop
    bool _explicitQuit;            // player used the 'quit' verb (skip end menu)
    bool _undoAvailable;           // a usable undo snapshot exists
    std::string _undoSnapshot;     // serialized state from before the last command
    std::string _initialSnapshot;  // serialized state at the first prompt (RESTART)
    strid_t _transcriptStream;
    frefid_t _transcriptRef;

public:
    // heapsort.cpp
    XArrayType H;

    // id_table.cpp
    XArrayType h_index;

    // keywords.cpp
    XArrayType Literals, Vocabulary;
    XArrayType Type_ID_List, Object_ID_List, Attribute_ID_List;

    // parser.cpp
    String Command;
    int Abbreviate;
    ListType Proximate;
    ListType verb_names;
    ListType object_names;

    // semantic.cpp
    XArrayType Type_List, Object_List;
    ListType Overlooked;
    StringPtr NullStr;

    // The game file (set by glk_main before runGame())
    Common::FileStream _gameFile;
    bool Translating;

private:
    bool initialize();
    void deinitialize();
    void interpret();

    void lookup(int the_obj, int the_attr, ResultType &result, ContextType &context, DesiredType desired);
    bool send_message(int transport, int message_sent, int recipient, ResultType &result,
        ContextType &context);

    void eval_expr(ExprTree the_expr, ResultType &result, ContextType &context, DesiredType desired);
    bool eval_condition(ExprTree the_expr, ContextType &context);
    void exec_stmt(StatementPtr the_stmt, ResultType &result, ContextType &context);

public:
    Archetype();

    void runGame();

    // Spatterlight launches games via glk_main and doesn't pass a save-slot
    // through ConfMan, so loadingSavegame is always false and
    // loadLauncherSavegame is a no-op. saveGame/loadGame drive a Glk file
    // prompt and route through save_game_state/load_game_state.
    bool loadingSavegame() const { return _saveSlot >= 0; }
    Common::Error loadLauncherSavegame() { _saveSlot = -2; return Common::Error(Common::kNoError); }
    Common::Error loadGame();
    Common::Error saveGame();

    bool shouldQuit() const { return _shouldQuit; }
    void quitGame() { _shouldQuit = true; }

    // The interpreter uses this for `? n` and `? string`. Returns a value in
    // [0..maxVal]. Like the other Spatterlight terps (Scott, TaylorMade, Plus,
    // Comprehend) this draws from the shared erkyrath_random() so a fixed seed
    // makes the regression walkthroughs replay identically on every host.
    int getRandomNumber(int maxVal) {
        if (maxVal < 0) return 0;
        return (int)(erkyrath_random() % (glui32)(maxVal + 1));
    }

    template<class... TParam>
    void write(const String &fmt, TParam... args);

    template<class... TParam>
    void writeln(const String &fmt, TParam... args);
    void writeln() { writeln(""); }

    String readLine();
    char readKey();

private:
    void write_internal(const String *fmt, ...);
    void writeln_internal(const String *fmt, ...);

    // Meta-command helpers (see the ReentryMode block above).
    String rawReadLine();
    bool handleMetaCommand(const String &cmd);
    void toggleTranscript(bool on);
    void captureSnapshot(std::string &out);
    bool restoreSnapshot(const std::string &in);
    int postGameMenu();
};

template<class... TParam>
inline void Archetype::write(const String &format, TParam... param) {
    return write_internal(&format, Common::forward<TParam>(param)...);
}

template<class... TParam>
inline void Archetype::writeln(const String &format, TParam... param) {
    return writeln_internal(&format, Common::forward<TParam>(param)...);
}

extern Archetype *g_vm;

} // End of namespace Archetype
} // End of namespace Glk

#endif

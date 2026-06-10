/* Comprehend interpreter port for Spatterlight.
 *
 * Original engine: ScummVM engines/glk/comprehend/.
 * Same approach as terps/archetype/archetype.h: drop the GlkAPI base class
 * and just call the Glk C API directly.
 */

#ifndef COMPREHEND_COMPREHEND_H
#define COMPREHEND_COMPREHEND_H

#include "comprehend_compat.h"
#include "game.h"

extern "C" {
#include "glk.h"
}

// Window rock IDs — match the values used by scott and taylormade.
#define GLK_BUFFER_ROCK   1
#define GLK_STATUS_ROCK   1010
#define GLK_GRAPHICS_ROCK 1020

namespace Glk {
namespace Comprehend {

class DrawSurface;
class Pics;

#define EXTRA_STRING_TABLE(x) (0x8200 | (x))

struct GameStrings {
    uint16 game_restart;
};

class Comprehend {
private:
    int _saveSlot;
    bool _graphicsEnabled;
    bool _disableSaves;
    bool _shouldQuit;
    Common::String _gameId;
    Common::String _gameFile;

public:
    winid_t _topWindow;     // graphics window (or null if disabled)
    winid_t _statusWindow;  // text grid showing current room description (always visible)
    winid_t _bottomWindow;  // main text buffer
    DrawSurface *_drawSurface;
    ComprehendGame *_game;
    Pics *_pics;
    uint _drawFlags;

    // Pixel scale used when blitting our 280x160 render buffer to the Glk
    // graphics window. Matches scott's PutPixel-style approach.
    int _pixelSize;

public:
    Comprehend();
    ~Comprehend();

    // glk_main calls these before runGame so the engine knows what to load.
    void setGameId(const Common::String &gameId) { _gameId = gameId; }
    void setGameFile(const Common::String &path) { _gameFile = path; }
    const Common::String &getGameID() const { return _gameId; }
    const Common::String &getGameFile() const { return _gameFile; }

    void runGame();

    // Stub savegame integration; Spatterlight doesn't pass save slots in.
    bool loadingSavegame() const { return _saveSlot >= 0; }
    bool loadLauncherSavegameIfNeeded() { _saveSlot = -1; return false; }
    Common::Error loadGame() { return Common::Error(Common::kNoGameDataFoundError); }
    Common::Error saveGame() { return Common::Error(Common::kNoGameDataFoundError); }
    Common::Error readSaveData(Common::SeekableReadStream *) { return Common::Error(Common::kNoGameDataFoundError); }
    Common::Error writeGameData(Common::WriteStream *) { return Common::Error(Common::kNoGameDataFoundError); }

    // ScummVM's launcher passes save slots to/from games via these; we
    // don't have a launcher, but the engine references them when the
    // player runs SAVE / RESTORE. Both fail silently for now.
    Common::Error saveGameState(int /*slot*/, const Common::String & /*desc*/) {
        return Common::Error(Common::kNoGameDataFoundError);
    }
    Common::Error loadGameState(int /*slot*/) {
        return Common::Error(Common::kNoGameDataFoundError);
    }

    bool shouldQuit() const { return _shouldQuit; }
    void quitGame() { _shouldQuit = true; }
    int getRandomNumber(int maxVal) {
        if (maxVal <= 0) return 0;
        return std::rand() % (maxVal + 1);
    }

    bool canLoadGameStateCurrently(Common::U32String * = nullptr) { return false; }
    bool canSaveGameStateCurrently(Common::U32String * = nullptr) { return false; }
    void setDisableSaves(bool flag) { _disableSaves = flag; }

    // Output (mirrors GlkAPI's print/printRoomDesc/readLine API).
    void print(const char *fmt, ...) GCC_PRINTF(2, 3);
    template<class... TParam>
    void print(const Common::U32String &fmt, TParam... param);

    void printRoomDesc(const Common::String &desc);
    void readLine(char *buffer, size_t maxLen);
    int readChar();

    // Graphics — render to _drawSurface then blit to _topWindow.
    void drawPicture(uint pictureNum);
    void drawLocationPicture(int pictureNum, bool clearBg = true);
    void drawItemPicture(int pictureNum);
    void clearScreen(bool isBright);
    bool toggleGraphics();
    void showGraphics();
    // Logical text/graphics-mode switch that keeps the picture window open
    // (used by Talisman's mode-switch opcodes), unlike toggleGraphics() which
    // genuinely opens/closes the window for the player's manual toggle.
    void setGraphicsMode(bool graphicsMode);
    bool isGraphicsEnabled() const { return _graphicsEnabled; }

    ComprehendGame *getGame() const { return _game; }
    bool isInputLineActive() const { return false; }

    // Blits the current state of _drawSurface to the Glk graphics window,
    // pixel-by-pixel via glk_window_fill_rect. Called by drawPicture().
    void blitSurfaceToWindow();

private:
    void initialize();
    void deinitialize();
    void createGame();
    void print_u32_internal(const Common::U32String *fmt, ...);
};

template<class... TParam>
void Comprehend::print(const Common::U32String &fmt, TParam... param) {
    print_u32_internal(&fmt, Common::forward<TParam>(param)...);
}

extern Comprehend *g_comprehend;
// Aliased to g_comprehend so the engine's `g_vm` references (in pics.cpp etc.) keep working.
#define g_vm g_comprehend

// Stub for the original Debugger console. The engine references
// g_debugger->_invLimit (toggles inventory weight checks) and
// g_debugger->dumpInstruction (debug print of executing opcodes); we ship
// the defaults (limit on, no dump) and that's the whole production behaviour.
struct DebuggerStub {
    bool _invLimit = true;
    Common::String dumpInstruction(class ComprehendGame *, struct FunctionState *,
                                   const struct Instruction *) {
        return Common::String();
    }
};
extern DebuggerStub *g_debugger;

} // namespace Comprehend
} // namespace Glk

#endif

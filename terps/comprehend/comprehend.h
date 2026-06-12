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
    // Last room description shown in the status grid, kept so we can re-wrap it
    // to the new width when the window is resized.
    Common::String _lastRoomDesc;

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

    // Called by game_save() / game_restore() when the player types SAVE/RESTORE.
    Common::Error saveGameState(int slot, const Common::String &desc);
    Common::Error loadGameState(int slot);

    bool shouldQuit() const { return _shouldQuit; }
    void quitGame() { _shouldQuit = true; }
    int getRandomNumber(int maxVal) {
        if (maxVal <= 0) return 0;
        return std::rand() % (maxVal + 1);
    }

    bool canLoadGameStateCurrently(Common::U32String * = nullptr) { return _game != nullptr; }
    bool canSaveGameStateCurrently(Common::U32String * = nullptr) { return !_disableSaves && _game != nullptr; }
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
    // Suppress the animated slow-draw reveal for the next drawPicture() calls
    // (used when repainting an unchanged scene — see _suppressSlowDraw).
    void setSuppressSlowDraw(bool suppress) { _suppressSlowDraw = suppress; }
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
    // Blits only Apple rows [y0,y1] of _drawSurface (used by the slow-draw
    // reveal to repaint just the band it changed this tick).
    void blitSurfaceRowsToWindow(int y0, int y1);

    // Window-resize / repaint handling (evtype_Arrange / evtype_Redraw), called
    // from the input-wait loops: rescale the picture to the new window, repaint
    // it, and re-wrap the status description.
    void onArrange();
    // Pick the largest integer pixel scale of the 280x160 render that fits the
    // window (width, and up to ~60% of the screen height), resizing the graphics
    // window to suit. Returns true if the scale changed.
    bool recomputeGraphicsScale();

private:
    // True while a slow-draw reveal is running in the background (timer events
    // are active and the input loops are advancing it tick by tick).
    bool _slowDrawActive;

    // When set, the next drawPicture() paints instantly even if slow-draw is on.
    // update_graphics() sets this while repainting a scene identical to the one
    // already on screen (common: an action redraws the room without changing it)
    // so we don't re-animate the same image, which just looks like a stutter.
    bool _suppressSlowDraw;

    // Advance one timer tick of the background slow-draw reveal and blit the
    // changed rows. Cancels the timer and clears _slowDrawActive when done.
    void tickSlowDraw();
    // Complete the slow-draw reveal immediately (called before starting a new
    // picture or when the window is resized). Updates _drawSurface to the final
    // fully-drawn page; the caller is responsible for blitting to the window.
    void finishSlowDraw();

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

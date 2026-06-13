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

    // Count of consecutive newlines just written to the bottom buffer window.
    // Used to collapse runs of newlines so the engine (which is liberal with
    // '\n') never emits more than two in a row -- i.e. at most one blank line.
    // Starts at 2 so any leading newlines before the first real output are
    // suppressed entirely.
    int _trailingNewlines = 2;

    // Open transcript file stream (echo of the bottom window), or null. Driven
    // by the #transcript metacommand.
    strid_t _transcript = nullptr;

    // When set, print() output is sent to the transcript only, bypassing the
    // buffer window. Used for room descriptions, which are shown in the status
    // grid rather than the scroll-back buffer but still belong in a log.
    bool _redirectToTranscript = false;
    // Separate trailing-newline counter for the transcript-redirect path, so it
    // doesn't clobber the buffer window's collapse state.
    int _transcriptTrailingNewlines = 2;

    // Undo history: serialized game-state snapshots, one per turn (oldest
    // first). The back() entry mirrors the current state; #undo drops it and
    // restores the entry below. Capped at kMaxUndo to bound memory.
    static const size_t kMaxUndo = 100;
    std::vector<std::vector<byte> > _undoStack;
    bool serializeGameState(std::vector<byte> &out);
    void deserializeGameState(const std::vector<byte> &in);

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
    void setCentered(bool on);
    void readLine(char *buffer, size_t maxLen);
    int readChar();

    // Graphics — render to _drawSurface then blit to _topWindow.
    void drawPicture(uint pictureNum);
    void drawLocationPicture(int pictureNum, bool clearBg = true);
    void drawItemPicture(int pictureNum);
    void clearScreen(bool isBright);
    // Paint a whole scene that replaces the screen (a background-clearing
    // location plus any item pictures layered on top). If the requested set of
    // pictures is identical to what is already composited, the reveal is skipped
    // and the scene is painted instantly — see _screenComposition.
    void paintBackground(const Common::Array<uint> &pics);
    // Layer pictures on top of the current screen without clearing it. Pictures
    // already present are repainted instantly rather than re-animated.
    void paintOverlay(const Common::Array<uint> &pics);
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

    // When set, drawPicture() paints instantly even if slow-draw is on. The
    // paintBackground()/paintOverlay() scene helpers set this while repainting a
    // scene identical to the one already on screen (common: an action redraws
    // the room without changing it, or Talisman's OPCODE_DRAW_ROOM repaints the
    // current room after a speech) so we don't re-animate the same image, which
    // just looks like a stutter.
    bool _suppressSlowDraw;

    // The pictures currently composited on screen, in paint order: a background
    // (location / dark / bright) followed by any item pictures layered on top.
    // Used by paintBackground()/paintOverlay() to detect repaints that change
    // nothing. Maintained across every draw path (per-turn refresh, explicit
    // draw opcodes, scripted cutscenes) so they share one notion of the screen.
    Common::Array<uint> _screenComposition;

    // True while The Coveted Mirror's per-turn grain-fall animation is running.
    // Shares the same Glk timer as the slow-draw reveal; the input loops advance
    // whichever is active. _hgTickAccum subdivides the fast slow-draw tick so the
    // grain steps at a visible pace.
    bool _hourglassFalling = false;
    int _hgTickAccum = 0;

    // Advance one timer tick of the background slow-draw reveal and blit the
    // changed rows. Cancels the timer and clears _slowDrawActive when done.
    void tickSlowDraw();
    // Advance the grain-fall animation (paced down from the timer tick) and blit
    // its dirty band. Clears _hourglassFalling when the fall completes.
    void tickHourglass();
    // Start the grain-fall animation if gmDrawCMHourglass() flagged a single-grain
    // drop this turn -- but only with the picture window open, slow-draw enabled,
    // and no room reveal already running (the room paint-in owns the timer then,
    // and the hourglass just snaps, as the original does on a room change).
    void maybeStartHourglassFall();
    // Request the Glk timer iff either background animation needs it.
    void updateTimerRequest();
    // Complete the slow-draw reveal immediately (called before starting a new
    // picture or when the window is resized). Updates _drawSurface to the final
    // fully-drawn page; the caller is responsible for blitting to the window.
    void finishSlowDraw();

    void initialize();
    void deinitialize();
    void createGame();
    void print_u32_internal(const Common::U32String *fmt, ...);

    // Write to the bottom window stream, collapsing consecutive newlines into
    // a single one (tracking state across calls via _lastBottomChar).
    void putBottom(const char *s);
    void putBottomUni(const glui32 *s);

public:
    // #transcript metacommand: echo the bottom-window output to a text file.
    // transcript() toggles (or follows an explicit "on"/"off" argument).
    void transcript(const char *arg);
    void transcriptOn();
    void transcriptOff();

    // Route subsequent print() output to the transcript only (true) or back to
    // the buffer window (false). See _redirectToTranscript.
    void redirectOutputToTranscript(bool on) { _redirectToTranscript = on; }

    // #undo support. pushUndo() records the current state as a turn boundary
    // (deduplicated against the last snapshot); undo() reverts to the previous
    // turn's state, returning false (with no change) if there is nothing to
    // undo.
    void pushUndo();
    bool undo();
    void clearUndo() { _undoStack.clear(); }
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

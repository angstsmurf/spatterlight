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
    // graphics window. Matches scott's PutPixel-style approach. In double
    // hi-res mode this is the vertical pixel height; the horizontal column step
    // is halved (560 columns over the same physical width) -- see pixelStepX().
    int _pixelSize;

    // Apple II double hi-res ("<D>") mode. When true the draw surface is 560
    // wide and the DHGR renderer is used; otherwise standard 280-wide hi-res.
    bool _useDhgr = false;

    // IBM PCjr / Tandy 16-colour mode. When true (and the JR_GRAPH.OVR drawing
    // tables loaded) DOS v1 pictures route through graphics_magician_pcjr.cpp
    // (BIOS mode 9, 320x200x16) instead of the 4-colour CGA renderer, mirroring
    // the original NOVEL.EXE's PCjr branch. Opt-in via the #pcjr command.
    bool _usePcjr = false;

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

    // Slot-based save/load (used by Spatterlight's autosave/launcher hooks).
    Common::Error saveGameState(int slot, const Common::String &desc);
    Common::Error loadGameState(int slot);

    // Called by game_save() / game_restore() when the player types SAVE/RESTORE.
    // Both prompt the player for a file via the standard Glk save/restore dialog.
    Common::Error saveGamePrompt();
    Common::Error loadGamePrompt();

private:
    // Save/restore live game state to/from an already-created fileref (shared
    // by the slot-based and prompt-based paths). The file carries a small
    // header so a wrong/corrupt file is rejected instead of crashing the game.
    Common::Error saveToFileref(frefid_t fref);
    Common::Error loadFromFileref(frefid_t fref);

public:

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
    // Redraw just The Coveted Mirror's right-hand hourglass panel at the current
    // sand level (VM variable 0x11), without touching the room picture, and arm
    // the freed-grain fall. The room only repaints when it changes, so this gives
    // the hourglass its own per-turn update -- the draining sand (and the grain
    // fall) is then visible on turns where the player stays put, matching the
    // original's per-turn hourglass step. A no-op outside CM / before the panel
    // is built. Call only on turns with no full room repaint queued (that path
    // draws the hourglass itself, snapping on the room change).
    void refreshCMHourglass();
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

    // Apple II double hi-res mode. on==true widens the draw surface to 560 and
    // routes pictures through the DHGR renderer; on==false restores standard
    // 280-wide hi-res. Resizes the surface and repaints the current picture.
    void setDhgrMode(bool on);
    bool dhgrMode() const { return _useDhgr; }
    // IBM PCjr 16-colour mode (DOS v1 games). on==true routes pictures through
    // the PCjr renderer (graphics_magician_pcjr.cpp); on==false restores the
    // 4-colour CGA renderer. Repaints the current picture.
    void setPcjrMode(bool on);
    bool pcjrMode() const { return _usePcjr; }
    // Horizontal width (in window pixels) of one render column: full _pixelSize
    // for standard hi-res, half for the 560-wide DHGR surface so the picture
    // keeps the same physical size/aspect.
    int pixelStepX() const { return _useDhgr ? (_pixelSize > 1 ? _pixelSize / 2 : 1) : _pixelSize; }
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
    // Set when a grain fall is armed during a room reveal (a move): the reveal
    // owns the timer and the panel band, so the fall is held until the reveal
    // finishes (tickSlowDraw) and then started -- this is what makes the grain
    // drop visible on move turns too, not only when the player stays put.
    bool _hourglassFallPending = false;

    // Advance one timer tick of the background slow-draw reveal and blit the
    // changed rows. Cancels the timer and clears _slowDrawActive when done.
    void tickSlowDraw();
    // Advance the grain-fall animation (paced down from the timer tick) and blit
    // its dirty band. Clears _hourglassFalling when the fall completes.
    void tickHourglass();
    // Start the grain-fall animation if gmDrawCMHourglass() flagged a single-grain
    // drop this turn -- with the picture window open and slow-draw enabled. If a
    // room reveal is running (a move), defer it via _hourglassFallPending until
    // the reveal finishes rather than dropping it, so the grain falls every turn.
    void maybeStartHourglassFall();
    // Actually kick off the grain fall (shared by the immediate and deferred
    // paths). Assumes the preconditions in maybeStartHourglassFall() are met.
    void beginHourglassFall();
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

/* Comprehend engine glue — Spatterlight port. */

#include "comprehend.h"
#include "graphics_magician.h"
#include "hdos_talisman.h"
#include "draw_surface.h"
#include "game.h"
#include "game_cc.h"
#include "game_data.h"
#include "game_oo.h"
#include "game_tm.h"
#include "game_tr1.h"
#include "game_tr2.h"
#include "pics.h"

namespace Glk {
namespace Comprehend {

// Same 2x scale as the ScummVM port: the engine renders at 280x160 and we
// blit at 2x to give comfortable on-screen pictures.
#define SCALE_FACTOR 2

// Apple II Talisman slow-draw pacing. The byte count per tick pairs with the
// tick interval — change them together. Matches the Apple II Scott Adams
// renderer's feel (10 ms ticks, tens of bytes a tick).
#define GM_SLOW_TICK_MS       10
#define GM_SLOW_BYTES_PER_TICK 50

// Spatterlight user settings, exported by glkimp (the headless build defines
// them as 0 in cheapglk). gli_slowdraw enables the animated picture reveal;
// gli_determinism (regression/replay mode) must suppress it for stable output.
extern "C" int gli_slowdraw;
extern "C" int gli_determinism;

// Total content area in pixels, exported by glkimp (0 in the headless build).
// Used to keep a rescaled picture from eating the whole window vertically.
extern "C" int gscreenh;

Comprehend *g_comprehend;

static DebuggerStub s_debuggerStub;
DebuggerStub *g_debugger = &s_debuggerStub;

Comprehend::Comprehend() :
    _saveSlot(-1), _graphicsEnabled(true), _disableSaves(false), _shouldQuit(false),
    _topWindow(nullptr), _statusWindow(nullptr), _bottomWindow(nullptr),
    _drawSurface(nullptr), _game(nullptr), _pics(nullptr), _drawFlags(0),
    _pixelSize(SCALE_FACTOR), _slowDrawActive(false), _suppressSlowDraw(false) {
    g_comprehend = this;
}

Comprehend::~Comprehend() {
    delete _drawSurface;
    delete _game;
    delete _pics;
    g_comprehend = nullptr;
}

// Thin wrappers so the Serializer can drive a GLK file stream directly.
struct GlkWriteStream : public Common::WriteStream {
    strid_t _str;
    explicit GlkWriteStream(strid_t str) : _str(str) {}
    void writeByte(byte b) override { glk_put_char_stream(_str, (unsigned char)b); }
    uint32 write(const void *p, uint32 n) override {
        glk_put_buffer_stream(_str, (char *)p, n);
        return n;
    }
};

struct GlkReadStream : public Common::SeekableReadStream {
    strid_t _str;
    int64 _size;
    bool _eos = false;
    GlkReadStream(strid_t str, int64 size) : _str(str), _size(size) {}
    byte readByte() override {
        glsi32 c = glk_get_char_stream(_str);
        if (c < 0) { _eos = true; return 0; }
        return (byte)c;
    }
    uint32 read(void *p, uint32 n) override {
        glui32 got = glk_get_buffer_stream(_str, (char *)p, n);
        if (got < n) _eos = true;
        return got;
    }
    bool eos() const override { return _eos; }
    int64 pos() const override { return (int64)glk_stream_get_position(_str); }
    int64 size() const override { return _size; }
    bool seek(int64 o, int w) override {
        glk_stream_set_position(_str, (glsi32)o, (glui32)w);
        _eos = false;
        return true;
    }
};

Common::Error Comprehend::saveGameState(int slot, const Common::String & /*desc*/) {
    Common::String name = Common::String::format("%s_save_%d", _gameId.c_str(), slot);
    frefid_t fref = glk_fileref_create_by_name(
        fileusage_SavedGame | fileusage_BinaryMode, (char *)name.c_str(), 0);
    if (!fref) return Common::Error(Common::kNoGameDataFoundError);

    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    glk_fileref_destroy(fref);
    if (!str) return Common::Error(Common::kNoGameDataFoundError);

    GlkWriteStream ws(str);
    Common::Serializer s(nullptr, &ws);
    _game->synchronizeSave(s);

    glk_stream_close(str, nullptr);
    return Common::Error(Common::kNoError);
}

Common::Error Comprehend::loadGameState(int slot) {
    Common::String name = Common::String::format("%s_save_%d", _gameId.c_str(), slot);
    frefid_t fref = glk_fileref_create_by_name(
        fileusage_SavedGame | fileusage_BinaryMode, (char *)name.c_str(), 0);
    if (!fref) return Common::Error(Common::kNoGameDataFoundError);

    if (!glk_fileref_does_file_exist(fref)) {
        glk_fileref_destroy(fref);
        return Common::Error(Common::kNoGameDataFoundError);
    }

    strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
    glk_fileref_destroy(fref);
    if (!str) return Common::Error(Common::kNoGameDataFoundError);

    // Seek to end to determine file size, then rewind.
    glk_stream_set_position(str, 0, seekmode_End);
    int64 size = (int64)glk_stream_get_position(str);
    glk_stream_set_position(str, 0, seekmode_Start);

    GlkReadStream rs(str, size);
    Common::Serializer s(&rs, nullptr);
    _game->synchronizeSave(s);

    glk_stream_close(str, nullptr);
    return Common::Error(Common::kNoError);
}

void Comprehend::runGame() {
    initialize();
    createGame();
    if (_game) {
        _game->loadGame();
        _game->playGame();
    }
    deinitialize();
}

void Comprehend::initialize() {
    _bottomWindow = glk_window_open(nullptr, 0, 0, wintype_TextBuffer, GLK_BUFFER_ROCK);
    glk_set_window(_bottomWindow);
    _statusWindow = glk_window_open(_bottomWindow,
        winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, GLK_STATUS_ROCK);
    showGraphics();   // closes and reopens _statusWindow around the graphics window

    _drawSurface = new DrawSurface();
    _pics = new Pics();
}

void Comprehend::deinitialize() {
    // Deliberately do NOT close the windows here. runGame() calls this once,
    // right before the process exits, so closing every window only wipes the
    // final screen (e.g. Talisman's "THE END" / death-menu text and picture)
    // before the player can read it. Leaving the windows open lets Glk's
    // implicit exit keep the last frame on screen until the player dismisses
    // it -- the normal Glk convention. The OS reclaims the windows on exit.
}

void Comprehend::createGame() {
    if (_gameId == "crimsoncrown")        _game = new CrimsonCrownGame();
    else if (_gameId == "ootopos")        _game = new OOToposGame();
    else if (_gameId == "talisman")       _game = new TalismanGame();
    else if (_gameId == "transylvania")   _game = new TransylvaniaGame1();
    else if (_gameId == "transylvaniav2") _game = new TransylvaniaGame2();
    else                                  error("Unknown game id '%s'", _gameId.c_str());

    // The Apple II hi-res renderer (graphics_magician.cpp) handles two Graphics
    // Magician dialects, both validated pixel-exact vs MAME. Talisman and OO-Topos
    // use op15 fill-bounds sub-ops (newer); Transylvania and Crimson Crown never
    // emit op15, end images on op7, and seed background fills in the bottom text
    // rows, so they need the full-screen fill clip (the legacy flag).
    gmSetLegacyFormat(_gameId == "transylvania" || _gameId == "crimsoncrown");
}

void Comprehend::putBottom(const char *s) {
    strid_t stream = glk_window_get_stream(_bottomWindow);
    for (; *s; ++s) {
        if (*s == '\n') {
            if (_trailingNewlines >= 2)
                continue;  // already a blank line; collapse further newlines
            ++_trailingNewlines;
        } else {
            _trailingNewlines = 0;
        }
        glk_put_char_stream(stream, (unsigned char)*s);
    }
}

void Comprehend::putBottomUni(const glui32 *s) {
    strid_t stream = glk_window_get_stream(_bottomWindow);
    for (; *s; ++s) {
        if (*s == '\n') {
            if (_trailingNewlines >= 2)
                continue;  // already a blank line; collapse further newlines
            ++_trailingNewlines;
        } else {
            _trailingNewlines = 0;
        }
        glk_put_char_stream_uni(stream, *s);
    }
}

void Comprehend::print(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    Common::String msg = Common::String::vformat(fmt, argp);
    va_end(argp);
    putBottom(msg.c_str());
}

void Comprehend::print_u32_internal(const Common::U32String *fmt, ...) {
    Common::U32String out;
    va_list argp;
    va_start(argp, fmt);
    Common::U32String::vformat(fmt->begin(), fmt->end(), out, argp);
    va_end(argp);

    // Spatterlight's Glk supports glk_put_string_stream_uni; output 32-bit
    // codepoints directly.
    putBottomUni((glui32 *)out.u32_str());
}

void Comprehend::printRoomDesc(const Common::String &desc) {
    if (!_statusWindow) return;

    // Remember it so onArrange() can re-wrap it after a resize.
    _lastRoomDesc = desc;

    glui32 width = 0;
    glk_window_get_size(_statusWindow, &width, nullptr);
    Common::String str = desc;
    if (width > 2)
        str.wordWrap(width - 2);
    str += '\n';

    // Size the window to the wrapped text plus one blank line at the bottom,
    // used as a delimiter (as UnQuill and others do), rather than leaving it at
    // a fixed height with empty rows below the description.
    int lines = 0;
    for (uint i = 0; i < str.size(); ++i)
        if (str[i] == '\n') ++lines;
    winid_t parent = glk_window_get_parent(_statusWindow);
    if (parent)
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                   (glui32)(lines + 1), _statusWindow);

    glk_window_clear(_statusWindow);

    while (!str.empty()) {
        size_t idx = str.findFirstOf('\n');
        if (idx == std::string::npos) idx = str.size() - 1;
        Common::String line = Common::String::format(" %s",
            Common::String(str.c_str(), str.c_str() + idx + 1).c_str());
        glk_put_string_stream(glk_window_get_stream(_statusWindow),
                              (char *)line.c_str());
        str = Common::String(str.c_str() + idx + 1);
    }

    // Fill the bottom row with underscores as a delimiter (as UnQuill does).
    glk_window_move_cursor(_statusWindow, 0, (glui32)lines);
    strid_t stream = glk_window_get_stream(_statusWindow);
    for (glui32 col = 0; col < width; ++col)
        glk_put_char_stream(stream, '_');
}

static FILE *scriptFile() {
    static FILE *f = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        const char *p = getenv("COMPREHEND_SCRIPT");
        if (p) f = fopen(p, "r");
    }
    return f;
}

void Comprehend::readLine(char *buffer, size_t maxLen) {
    if (scriptFile()) {
        if (!fgets(buffer, (int)maxLen, scriptFile())) {
            // End of transcript: terminate the process outright. Feeding "quit"
            // here is not safe -- some games (e.g. Talisman) answer "quit" with
            // a Y/N confirmation read through readChar(), which in script mode
            // returns a space, declining the quit. The turn loop would then ask
            // for another line, hit EOF again, and spin forever emitting output.
            glk_exit();
        }
        size_t n = strlen(buffer);
        while (n && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r'))
            buffer[--n] = 0;
        glk_put_string(buffer);
        glk_put_char('\n');
        _trailingNewlines = 1;
        return;
    }
    event_t ev;
    glk_request_line_event(_bottomWindow, buffer, (glui32)(maxLen - 1), 0);
    for (;;) {
        glk_select(&ev);
        if (ev.type == evtype_LineInput) break;
        if (ev.type == evtype_Timer && _slowDrawActive)
            tickSlowDraw();
        else if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
            onArrange();
    }
    buffer[ev.val1] = 0;
    // Glk echoes the typed line plus a single newline to the buffer; reflect
    // that one trailing newline so the count stays accurate across the input.
    _trailingNewlines = 1;
}

int Comprehend::readChar() {
    if (scriptFile())
        return ' ';
    glk_request_char_event(_bottomWindow);
    setDisableSaves(true);
    event_t ev;
    ev.type = evtype_None;
    while (ev.type != evtype_CharInput) {
        glk_select(&ev);
        if (ev.type == evtype_Timer && _slowDrawActive)
            tickSlowDraw();
        else if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
            onArrange();
    }
    setDisableSaves(false);
    return ev.val1;
}

// --- Graphics: render then blit ---

// Pull a pixel out of _drawSurface in 0xRRGGBB form expected by Glk.
static uint32 surfaceColorAt(Glk::Comprehend::DrawSurface *ds, int x, int y);

void Comprehend::drawPicture(uint pictureNum) {
    if (!_topWindow || !_pics || !_drawSurface) return;

    // Ensure the pixel scale matches the actual window width before drawing.
    // recomputeGraphicsScale() is normally called from onArrange() on resize
    // events, but the first drawPicture() call (the title screen) runs before
    // any such event fires, leaving _pixelSize at the initial SCALE_FACTOR=2
    // which can make the image wider than the window and clip it on the right.
    recomputeGraphicsScale();

    // If a background reveal from the previous picture is still running
    // (e.g. the player moved before it finished), complete it instantly.
    // Must happen before gmSetSlowDraw/hdosSetSlowDraw, which would clear
    // s_recordOps and break the active-renderer check inside finishSlowDraw().
    finishSlowDraw();

    // Both Talisman renderers (Apple II hi-res and DOS CGA) can reveal a picture
    // progressively, the way the original painted the page. Record the draw
    // order when the user has slow-draw on (and we aren't replaying a
    // transcript). Only the renderer that actually runs records anything.
    //
    // _suppressSlowDraw is set by update_graphics() when it repaints a scene
    // identical to the one already on screen: there is nothing new to reveal, so
    // paint instantly rather than re-animate the same image (looks like a stutter).
    bool slow = gli_slowdraw && !gli_determinism && !_suppressSlowDraw;
    gmSetSlowDraw(slow);
    hdosSetSlowDraw(slow);

    // Render through the Pics opcode interpreter into the pixel buffer,
    // then blit the initial (blank or reset) state to the Glk window.
    _pics->renderPicture((int)pictureNum);
    blitSurfaceToWindow();

    // If the render queued a slow-draw reveal, start a Glk timer and let the
    // input loops (readLine / readChar) advance it tick by tick in the
    // background. The game proceeds immediately: room description is shown
    // and the prompt appears while the picture is still being revealed.
    if (hdosSlowDrawActive() || gmSlowDrawActive()) {
        glk_request_timer_events(GM_SLOW_TICK_MS);
        _slowDrawActive = true;
    }
}

// Advance one timer tick of the background slow-draw reveal.
void Comprehend::tickSlowDraw() {
    const bool hdos = hdosSlowDrawActive();
    bool more;
    if (hdos) {
        more = hdosAdvanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
        hdosBlitSlowToSurface((uint32 *)_drawSurface->getPixels(),
                              _drawSurface->w, _drawSurface->h);
        int y0, y1;
        if (hdosSlowConsumeDirty(&y0, &y1))
            blitSurfaceRowsToWindow(y0, y1);
    } else {
        more = gmAdvanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
        gmBlitSlowToSurface((uint32 *)_drawSurface->getPixels(),
                            _drawSurface->w, _drawSurface->h);
        int y0, y1;
        if (gmSlowConsumeDirty(&y0, &y1))
            blitSurfaceRowsToWindow(y0, y1);
    }
    if (!more) {
        glk_request_timer_events(0);
        _slowDrawActive = false;
    }
}

// Complete the slow-draw reveal instantly and sync _drawSurface to the final
// fully-drawn page. The caller is responsible for blitting to the window.
void Comprehend::finishSlowDraw() {
    if (!_slowDrawActive) return;
    glk_request_timer_events(0);
    _slowDrawActive = false;
    const bool hdos = hdosSlowDrawActive();
    if (hdos) hdosFinishSlowDraw(); else gmFinishSlowDraw();
    if (hdos)
        hdosBlitToSurface((uint32 *)_drawSurface->getPixels(),
                          _drawSurface->w, _drawSurface->h);
    else
        gmBlitToSurface((uint32 *)_drawSurface->getPixels(),
                        _drawSurface->w, _drawSurface->h);
}

// Paint a scene that replaces the whole screen. If the requested set of
// pictures matches what is already composited, suppress the slow-draw reveal so
// an unchanged repaint (e.g. the room redrawn each turn, or Talisman's
// OPCODE_DRAW_ROOM after a speech) appears instantly instead of re-animating.
void Comprehend::paintBackground(const Common::Array<uint> &pics) {
    if (pics.empty())
        return;
    _suppressSlowDraw = (pics == _screenComposition);
    _screenComposition = pics;
    for (uint i = 0; i < pics.size(); i++)
        drawPicture(pics[i]);
    _suppressSlowDraw = false;
}

// Layer pictures on top of the current screen without clearing it. Pictures
// already present are repainted, but only the reveal animation is skipped when
// nothing new is added (so an already-visible object isn't re-animated).
void Comprehend::paintOverlay(const Common::Array<uint> &pics) {
    if (pics.empty())
        return;
    bool allPresent = true;
    for (uint i = 0; i < pics.size(); i++) {
        bool present = false;
        for (uint j = 0; j < _screenComposition.size(); j++)
            if (_screenComposition[j] == pics[i]) { present = true; break; }
        if (!present) {
            allPresent = false;
            _screenComposition.push_back(pics[i]);
        }
    }
    _suppressSlowDraw = allPresent;
    for (uint i = 0; i < pics.size(); i++)
        drawPicture(pics[i]);
    _suppressSlowDraw = false;
}

void Comprehend::drawLocationPicture(int pictureNum, bool clearBg) {
    Common::Array<uint> pics;
    pics.push_back((uint)(pictureNum + (clearBg ? LOCATIONS_OFFSET : LOCATIONS_NO_BG_OFFSET)));
    if (clearBg)
        paintBackground(pics);
    else
        paintOverlay(pics);
}

void Comprehend::drawItemPicture(int pictureNum) {
    Common::Array<uint> pics;
    pics.push_back((uint)(pictureNum + ITEMS_OFFSET));
    paintOverlay(pics);
}

void Comprehend::clearScreen(bool isBright) {
    Common::Array<uint> pics;
    pics.push_back((uint)(isBright ? BRIGHT_ROOM : DARK_ROOM));
    paintBackground(pics);
}

bool Comprehend::toggleGraphics() {
    if (_topWindow) {
        glk_window_close(_topWindow, nullptr);
        _topWindow = nullptr;
        _graphicsEnabled = false;
        return false;
    } else {
        showGraphics();
        return true;
    }
}

void Comprehend::setGraphicsMode(bool graphicsMode) {
    // Switch logical mode without destroying the picture window, so the layout
    // doesn't reflow on every switch (Talisman flips modes several times during
    // its intro). Turning graphics off just blanks the window; turning it back
    // on lets the next update() redraw into the still-open window.
    if (graphicsMode) {
        // Only enter graphics mode if a picture window actually exists. When it
        // doesn't (headless build, or the player hid it), stay in text mode so
        // behaviour matches the old toggleGraphics()/showGraphics() path.
        if (_topWindow)
            _graphicsEnabled = true;
    } else {
        _graphicsEnabled = false;
        if (_topWindow)
            glk_window_clear(_topWindow);
    }
}

void Comprehend::showGraphics() {
    if (_topWindow) return;
    if (_statusWindow) {
        glk_window_close(_statusWindow, nullptr);
        _statusWindow = nullptr;
    }
    _topWindow = glk_window_open(_bottomWindow,
        winmethod_Above | winmethod_Fixed,
        (glui32)(G_RENDER_HEIGHT * _pixelSize),
        wintype_Graphics, GLK_GRAPHICS_ROCK);
    _graphicsEnabled = (_topWindow != nullptr);
    // Reopen status above _bottomWindow — inserts it between graphics and buffer.
    // Height is set to fit the room description on the next printRoomDesc().
    _statusWindow = glk_window_open(_bottomWindow,
        winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, GLK_STATUS_ROCK);
}

void Comprehend::blitSurfaceToWindow() {
    blitSurfaceRowsToWindow(0, G_RENDER_HEIGHT - 1);
}

// Blit Apple rows [y0,y1] (inclusive) of _drawSurface to the window. The slow
// reveal calls this with just the band it changed this tick; a full picture
// blit goes through blitSurfaceToWindow() above.
void Comprehend::blitSurfaceRowsToWindow(int y0, int y1) {
    if (!_topWindow || !_drawSurface) return;
    glui32 winW = 0, winH = 0;
    glk_window_get_size(_topWindow, &winW, &winH);
    if (winW == 0 || winH == 0) return;

    if (y0 < 0) y0 = 0;
    if (y1 > G_RENDER_HEIGHT - 1) y1 = G_RENDER_HEIGHT - 1;
    if (y0 > y1) return;

    // Centre the render horizontally (matches ScummVM's 20*SCALE_FACTOR x offset).
    int xOff = ((int)winW - G_RENDER_WIDTH * _pixelSize) / 2;
    if (xOff < 0) xOff = 0;

    // Run-length encode along each row so we issue one glk_window_fill_rect
    // per same-coloured horizontal run instead of one per pixel — about 30x
    // fewer Glk calls for typical Comprehend rooms.
    //
    // Pixels are 0xRRGGBBAA; an alpha of 0 marks the transparent background
    // left by DrawSurface::clear(0) for item / no-background images. We skip
    // those runs so overlay images compose onto the room already shown in the
    // window instead of wiping it to black. Room images clear to an opaque
    // colour first, so every pixel is drawn.
    for (int y = y0; y <= y1; ++y) {
        int x = 0;
        while (x < G_RENDER_WIDTH) {
            uint32 c = surfaceColorAt(_drawSurface, x, y);
            int x0 = x;
            do { ++x; } while (x < G_RENDER_WIDTH && surfaceColorAt(_drawSurface, x, y) == c);
            if ((c & 0xff) == 0)
                continue;   // transparent — leave the background untouched
            // Strip alpha and map to Glk's 0xRRGGBB.
            glui32 rgb = (glui32)((c >> 8) & 0xffffff);
            glk_window_fill_rect(_topWindow, rgb,
                xOff + x0 * _pixelSize,
                y * _pixelSize,
                (x - x0) * _pixelSize,
                _pixelSize);
        }
    }
}

bool Comprehend::recomputeGraphicsScale() {
    if (!_topWindow) return false;
    glui32 winW = 0, winH = 0;
    glk_window_get_size(_topWindow, &winW, &winH);
    if (winW == 0) return false;

    // Largest integer multiple of the 280-wide render that fits the window...
    int scale = (int)(winW / G_RENDER_WIDTH);
    // ...but never let the picture take more than ~60% of the screen height.
    if (gscreenh > 0) {
        int scaleByHeight = (gscreenh * 3 / 5) / G_RENDER_HEIGHT;
        if (scaleByHeight >= 1 && scale > scaleByHeight)
            scale = scaleByHeight;
    }
    if (scale < 1) scale = 1;
    if (scale == _pixelSize) return false;

    _pixelSize = scale;
    winid_t parent = glk_window_get_parent(_topWindow);
    if (parent)
        glk_window_set_arrangement(parent, winmethod_Above | winmethod_Fixed,
                                   (glui32)(G_RENDER_HEIGHT * _pixelSize), _topWindow);
    return true;
}

// Repaint after a window resize / redraw request: rescale and re-blit the
// current picture (the persistent surface still holds it — Talisman's full
// hi-res page is opaque, so this restores it exactly), and re-wrap the status
// description to the new width.
void Comprehend::onArrange() {
    // Complete any background reveal so the final image is what gets rescaled.
    finishSlowDraw();
    if (_topWindow) {
        recomputeGraphicsScale();
        glk_window_clear(_topWindow);
        blitSurfaceToWindow();
    }
    if (!_lastRoomDesc.empty())
        printRoomDesc(_lastRoomDesc);
}

} // namespace Comprehend
} // namespace Glk

// Need access to DrawSurface's pixel buffer — defined out-of-class so it
// sees the full type after draw_surface.h is included above.
#include "draw_surface.h"
namespace Glk {
namespace Comprehend {
static uint32 surfaceColorAt(DrawSurface *ds, int x, int y) {
    const uint32 *p = (const uint32 *)ds->getBasePtr(x, y);
    return *p;
}
} // namespace Comprehend
} // namespace Glk

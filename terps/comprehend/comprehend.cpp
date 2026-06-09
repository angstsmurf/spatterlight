/* Comprehend engine glue — Spatterlight port. */

#include "comprehend.h"
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

Comprehend *g_comprehend;

static DebuggerStub s_debuggerStub;
DebuggerStub *g_debugger = &s_debuggerStub;

Comprehend::Comprehend() :
    _saveSlot(-1), _graphicsEnabled(true), _disableSaves(false), _shouldQuit(false),
    _topWindow(nullptr), _roomDescWindow(nullptr), _bottomWindow(nullptr),
    _drawSurface(nullptr), _game(nullptr), _pics(nullptr), _drawFlags(0),
    _pixelSize(SCALE_FACTOR) {
    g_comprehend = this;
}

Comprehend::~Comprehend() {
    delete _drawSurface;
    delete _game;
    delete _pics;
    g_comprehend = nullptr;
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
    _bottomWindow = glk_window_open(nullptr, 0, 0, wintype_TextBuffer, 1);
    glk_set_window(_bottomWindow);

    showGraphics();

    _drawSurface = new DrawSurface();
    _pics = new Pics();
}

void Comprehend::deinitialize() {
    if (_topWindow)      glk_window_close(_topWindow, nullptr);
    if (_bottomWindow)   glk_window_close(_bottomWindow, nullptr);
    if (_roomDescWindow) glk_window_close(_roomDescWindow, nullptr);
}

void Comprehend::createGame() {
    if (_gameId == "crimsoncrown")        _game = new CrimsonCrownGame();
    else if (_gameId == "ootopos")        _game = new OOToposGame();
    else if (_gameId == "talisman")       _game = new TalismanGame();
    else if (_gameId == "transylvania")   _game = new TransylvaniaGame1();
    else if (_gameId == "transylvaniav2") _game = new TransylvaniaGame2();
    else                                  error("Unknown game id '%s'", _gameId.c_str());
}

void Comprehend::print(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    Common::String msg = Common::String::vformat(fmt, argp);
    va_end(argp);
    glk_put_string_stream(glk_window_get_stream(_bottomWindow), (char *)msg.c_str());
}

void Comprehend::print_u32_internal(const Common::U32String *fmt, ...) {
    Common::U32String out;
    va_list argp;
    va_start(argp, fmt);
    Common::U32String::vformat(fmt->begin(), fmt->end(), out, argp);
    va_end(argp);

    // Spatterlight's Glk supports glk_put_string_stream_uni; output 32-bit
    // codepoints directly.
    glk_put_string_stream_uni(glk_window_get_stream(_bottomWindow),
                              (glui32 *)out.u32_str());
}

void Comprehend::printRoomDesc(const Common::String &desc) {
    if (!_roomDescWindow) return;
    glk_window_clear(_roomDescWindow);

    glui32 width = 0;
    glk_window_get_size(_roomDescWindow, &width, nullptr);
    Common::String str = desc;
    if (width > 2)
        str.wordWrap(width - 2);
    str += '\n';

    while (!str.empty()) {
        size_t idx = str.findFirstOf('\n');
        if (idx == std::string::npos) idx = str.size() - 1;
        Common::String line = Common::String::format(" %s",
            Common::String(str.c_str(), str.c_str() + idx + 1).c_str());
        glk_put_string_stream(glk_window_get_stream(_roomDescWindow),
                              (char *)line.c_str());
        str = Common::String(str.c_str() + idx + 1);
    }
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
        return;
    }
    event_t ev;
    glk_request_line_event(_bottomWindow, buffer, (glui32)(maxLen - 1), 0);
    for (;;) {
        glk_select(&ev);
        if (ev.type == evtype_LineInput) break;
    }
    buffer[ev.val1] = 0;
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
    }
    setDisableSaves(false);
    return ev.val1;
}

// --- Graphics: render then blit ---

// Pull a pixel out of _drawSurface in 0xRRGGBB form expected by Glk.
static uint32 surfaceColorAt(Glk::Comprehend::DrawSurface *ds, int x, int y);

void Comprehend::drawPicture(uint pictureNum) {
    if (!_topWindow || !_pics || !_drawSurface) return;

    // Render through the Pics opcode interpreter into the pixel buffer,
    // then blit the buffer to the Glk graphics window.
    _pics->renderPicture((int)pictureNum);
    blitSurfaceToWindow();
}

void Comprehend::drawLocationPicture(int pictureNum, bool clearBg) {
    drawPicture((uint)(pictureNum + (clearBg ? LOCATIONS_OFFSET : LOCATIONS_NO_BG_OFFSET)));
}

void Comprehend::drawItemPicture(int pictureNum) {
    drawPicture((uint)(pictureNum + ITEMS_OFFSET));
}

void Comprehend::clearScreen(bool isBright) {
    drawPicture(isBright ? BRIGHT_ROOM : DARK_ROOM);
}

bool Comprehend::toggleGraphics() {
    if (_topWindow) {
        glk_window_close(_topWindow, nullptr);
        _topWindow = nullptr;
        _graphicsEnabled = false;
        _roomDescWindow = glk_window_open(_bottomWindow,
            winmethod_Above | winmethod_Fixed, 5, wintype_TextGrid, 1);
        return false;
    } else {
        if (_roomDescWindow) {
            glk_window_close(_roomDescWindow, nullptr);
            _roomDescWindow = nullptr;
        }
        showGraphics();
        return true;
    }
}

void Comprehend::showGraphics() {
    if (_topWindow) return;
    _topWindow = glk_window_open(_bottomWindow,
        winmethod_Above | winmethod_Fixed,
        (glui32)(G_RENDER_HEIGHT * _pixelSize),
        wintype_Graphics, 2);
    _graphicsEnabled = (_topWindow != nullptr);
}

void Comprehend::blitSurfaceToWindow() {
    if (!_topWindow || !_drawSurface) return;
    glui32 winW = 0, winH = 0;
    glk_window_get_size(_topWindow, &winW, &winH);
    if (winW == 0 || winH == 0) return;

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
    for (int y = 0; y < G_RENDER_HEIGHT; ++y) {
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

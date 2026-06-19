/* Comprehend engine glue — Spatterlight port. */

#include "comprehend.h"
#include "graphics_magician.h"
#include "graphics_magician_cga.h"
#include "graphics_magician_dhgr.h"
#include "graphics_magician_pcjr.h"
#include "draw_surface.h"
#include "game.h"
#include "game_cc.h"
#include "game_cm.h"
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

// The Coveted Mirror grain fall is paced down from the slow-draw tick: one grain
// step every HG_TICKS_PER_FRAME ticks (~70 ms), so the freed grain takes about
// half a second to fall through the neck.
#define HG_TICKS_PER_FRAME    7

// Spatterlight user settings, exported by glkimp (the headless build defines
// them as 0 in cheapglk). gli_slowdraw enables the animated picture reveal;
// gli_determinism (regression/replay mode) must suppress it for stable output.
extern "C" int gli_slowdraw;
extern "C" int gli_determinism;
// Comprehend preferred-graphics-mode: 0 = more colours (PCjr/DHGR), 1 = less.
extern "C" int gli_comprehend_graphics;

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

// Saved-game file header. Lets a restore reject a file that isn't one of this
// story's saves, instead of feeding garbage into the live game state (which
// then crashes when an out-of-range room/object is dereferenced).
static const char kSaveMagic[4] = { 'C', 'M', 'S', 'V' };
static const byte kSaveVersion = 1;

// Serialize the live game state out to an already-created fileref. Shared by
// the slot-based save (autosave/launcher) and the SAVE-verb prompt path.
Common::Error Comprehend::saveToFileref(frefid_t fref) {
    if (!fref) return Common::Error(Common::kNoGameDataFoundError);

    strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
    if (!str) return Common::Error(Common::kNoGameDataFoundError);

    GlkWriteStream ws(str);
    // Header: magic, version, then the game id (length-prefixed) so a restore
    // can confirm the file belongs to the running story.
    ws.write(kSaveMagic, sizeof(kSaveMagic));
    ws.writeByte(kSaveVersion);
    byte idLen = (byte)MIN(_gameId.size(), (size_t)255);
    ws.writeByte(idLen);
    ws.write(_gameId.c_str(), idLen);

    Common::Serializer s(nullptr, &ws);
    _game->synchronizeSave(s);

    glk_stream_close(str, nullptr);
    return Common::Error(Common::kNoError);
}

// Restore live game state from an already-created fileref. Validates the header
// and rolls back to the pre-restore state if the payload turns out to be bad,
// so a wrong or truncated file can never leave the game in a crashing state.
Common::Error Comprehend::loadFromFileref(frefid_t fref) {
    if (!fref) return Common::Error(Common::kNoGameDataFoundError);

    strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
    if (!str) return Common::Error(Common::kNoGameDataFoundError);

    // Seek to end to determine file size, then rewind.
    glk_stream_set_position(str, 0, seekmode_End);
    int64 size = (int64)glk_stream_get_position(str);
    glk_stream_set_position(str, 0, seekmode_Start);

    GlkReadStream rs(str, size);

    // Validate the header before touching live state.
    char magic[4] = { 0 };
    rs.read(magic, sizeof(magic));
    byte ver = rs.readByte();
    byte idLen = rs.readByte();
    char idBuf[256] = { 0 };
    if (idLen)
        rs.read(idBuf, idLen);
    bool headerOk = !rs.eos() &&
                    memcmp(magic, kSaveMagic, sizeof(kSaveMagic)) == 0 &&
                    ver == kSaveVersion &&
                    _gameId == Common::String(idBuf, idLen);
    if (!headerOk) {
        glk_stream_close(str, nullptr);
        print("That is not a saved game for this story.\n");
        return Common::Error(Common::kNoGameDataFoundError);
    }

    // Read the remaining bytes as the state payload.
    int64 payloadLen = size - (int64)(sizeof(magic) + 2 + idLen);
    std::vector<byte> payload(payloadLen > 0 ? (size_t)payloadLen : 0);
    if (!payload.empty())
        rs.read(payload.data(), (uint32)payload.size());
    glk_stream_close(str, nullptr);

    // The state layout is fixed-size for a given story, so the live state's
    // serialized length is exactly how long a genuine save's payload must be.
    // A truncated or otherwise wrong-sized file is rejected here, before it can
    // reach synchronizeSave() (which asserts on a mismatched room/item count).
    std::vector<byte> snapshot;
    bool haveSnapshot = serializeGameState(snapshot);
    if (!haveSnapshot || payload.size() != snapshot.size()) {
        print("That saved game appears to be damaged.\n");
        return Common::Error(Common::kNoGameDataFoundError);
    }

    deserializeGameState(payload);

    // A right-sized but internally inconsistent payload still shouldn't be able
    // to strand the player in a non-existent room: roll back if so.
    if (!_game->saveStateLooksValid()) {
        deserializeGameState(snapshot);
        print("That saved game appears to be damaged.\n");
        return Common::Error(Common::kNoGameDataFoundError);
    }

    clearUndo();  // can't undo across a restore

    // Repaint the room description, item list and picture for the restored
    // state. Without this the screen keeps showing the room the player restored
    // *from* until something else happens to dirty the graphics: the in-game
    // RESTORE verb (and the slot-based loadGameState) otherwise leave the flags
    // untouched. The launcher autorestore path already forces this in playGame().
    _game->_updateFlags = UPDATE_ALL;
    return Common::Error(Common::kNoError);
}

Common::Error Comprehend::saveGameState(int slot, const Common::String & /*desc*/) {
    Common::String name = Common::String::format("%s_save_%d", _gameId.c_str(), slot);
    frefid_t fref = glk_fileref_create_by_name(
        fileusage_SavedGame | fileusage_BinaryMode, (char *)name.c_str(), 0);
    Common::Error err = saveToFileref(fref);
    if (fref) glk_fileref_destroy(fref);
    return err;
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

    Common::Error err = loadFromFileref(fref);
    glk_fileref_destroy(fref);
    return err;
}

// SAVE verb: prompt the player for a file via the standard Glk save dialog.
Common::Error Comprehend::saveGamePrompt() {
    frefid_t fref = glk_fileref_create_by_prompt(
        fileusage_SavedGame | fileusage_BinaryMode, filemode_Write, 0);
    if (!fref) return Common::Error(Common::kNoGameDataFoundError);

    Common::Error err = saveToFileref(fref);
    glk_fileref_destroy(fref);
    return err;
}

// RESTORE verb: prompt the player for a file via the standard Glk restore dialog.
Common::Error Comprehend::loadGamePrompt() {
    frefid_t fref = glk_fileref_create_by_prompt(
        fileusage_SavedGame | fileusage_BinaryMode, filemode_Read, 0);
    if (!fref) return Common::Error(Common::kNoGameDataFoundError);

    Common::Error err = loadFromFileref(fref);
    glk_fileref_destroy(fref);
    return err;
}

bool Comprehend::saveStateToPath(const char *path) {
    std::vector<byte> snapshot;
    if (!serializeGameState(snapshot))
        return false;
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    // Trailer: the PRNG call count, so a restore can fast-forward the std::rand()
    // stream to the exact same position (some game logic is rand-gated).
    unsigned long rc = _randCalls;
    size_t n = snapshot.empty() ? 0 : fwrite(snapshot.data(), 1, snapshot.size(), f);
    n += fwrite(&rc, 1, sizeof(rc), f);
    fclose(f);
    return n == snapshot.size() + sizeof(rc);
}

bool Comprehend::loadStateFromPath(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    std::vector<byte> payload;
    byte buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        payload.insert(payload.end(), buf, buf + n);
    fclose(f);

    // Split off the PRNG-count trailer.
    unsigned long rc = 0;
    if (payload.size() < sizeof(rc))
        return false;
    memcpy(&rc, payload.data() + payload.size() - sizeof(rc), sizeof(rc));
    payload.resize(payload.size() - sizeof(rc));

    std::vector<byte> snapshot;
    if (!serializeGameState(snapshot) || payload.size() != snapshot.size())
        return false;
    deserializeGameState(payload);
    if (_game && !_game->saveStateLooksValid()) {
        deserializeGameState(snapshot);
        return false;
    }

    // Reproduce the live rand() position: the engine never seeds, so the stream
    // is the default srand(1) sequence; reset and burn rc draws to realign it.
    std::srand(1);
    for (unsigned long i = 0; i < rc; ++i)
        (void)std::rand();
    _randCalls = rc;
    clearUndo();
    if (_game)
        _game->_updateFlags = UPDATE_ALL;
    return true;
}

bool Comprehend::serializeGameState(std::vector<byte> &out) {
    if (!_game) return false;
    Common::MemoryReadWriteStream mem(Common::YES);
    Common::Serializer s(nullptr, &mem);
    _game->synchronizeSave(s);
    out.assign(mem.getData(), mem.getData() + (size_t)mem.size());
    return true;
}

void Comprehend::deserializeGameState(const std::vector<byte> &in) {
    if (!_game) return;
    // Let the game swap in a different static database before the payload is
    // applied (Talisman loads its part-2 desert tables when restoring a desert
    // save). Must run before synchronizeSave reads the rooms/items into them.
    _game->prepareRestore(in);
    Common::MemoryReadWriteStream mem(Common::YES);
    mem.write(in.data(), (uint32)in.size());
    mem.seek(0, SEEK_SET);
    Common::Serializer s(&mem, nullptr);
    _game->synchronizeSave(s);
    gmCMHourglassReset();  // undo jumps the sand back; snap, don't animate
    _hourglassFallPending = false;
}

void Comprehend::pushUndo() {
    std::vector<byte> snapshot;
    if (!serializeGameState(snapshot))
        return;
    // Don't record an identical, back-to-back snapshot (e.g. when a turn is
    // re-run via REDO_TURN), so each entry is a distinct turn boundary.
    if (!_undoStack.empty() && _undoStack.back() == snapshot)
        return;
    _undoStack.push_back(snapshot);
    if (_undoStack.size() > kMaxUndo)
        _undoStack.erase(_undoStack.begin());
}

bool Comprehend::undo() {
    // back() mirrors the current state; we need at least one earlier state to
    // revert to.
    if (_undoStack.size() < 2)
        return false;
    _undoStack.pop_back();
    deserializeGameState(_undoStack.back());
    return true;
}

bool Comprehend::undoTurn(uint turns) {
    if (_undoStack.empty())
        return false;
    // back() is the start of the fatal turn (one turn back). Each additional
    // turn drops one more boundary off the top, but always keep one snapshot to
    // restore. After restoring, back() mirrors the restored state, matching the
    // invariant the rest of the undo machinery relies on.
    while (turns > 1 && _undoStack.size() > 1) {
        _undoStack.pop_back();
        --turns;
    }
    deserializeGameState(_undoStack.back());
    return true;
}

void Comprehend::applyPreferredGraphicsMode() {
    if (gli_comprehend_graphics == 0) {
        // More colours: enable PCjr or DHGR when drawing tables are available.
        if (gmpcjrHaveDrawingTables())
            setPcjrMode(true);
        else if (Common::DiskImageFS::active() && gmDhgrHaveDrawingTables())
            setDhgrMode(true);
    }
    // Less colours (gli_comprehend_graphics == 1): leave defaults (CGA / hi-res).
}

void Comprehend::runGame() {
    initialize();
    createGame();
    if (_game) {
        _game->loadGame();
        applyPreferredGraphicsMode();
        _game->playGame();
    }
    deinitialize();
}

void Comprehend::initialize() {
    // Centred style for text the original interpreter centres on screen (e.g.
    // the OO-Topos title credits). Hints must be set before the window opens.
    glk_stylehint_set(wintype_TextBuffer, style_User1,
                      stylehint_Justification, stylehint_just_Centered);

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
    else if (_gameId == "covetedmirror")  _game = new CovetedMirrorGame();
    else                                  error("Unknown game id '%s'", _gameId.c_str());

    // The Apple II hi-res renderer (graphics_magician.cpp) handles two Graphics
    // Magician dialects, both validated pixel-exact vs MAME. Talisman and OO-Topos
    // use op15 fill-bounds sub-ops (newer); Transylvania and Crimson Crown never
    // emit op15, end images on op7, and seed background fills in the bottom text
    // rows, so they need the full-screen fill clip (the legacy flag).
    gmSetLegacyFormat(_gameId == "transylvania" || _gameId == "crimsoncrown");
}

void Comprehend::putBottom(const char *s) {
    // In redirect mode, send the text to the transcript only (or nowhere, if no
    // transcript is open); otherwise to the buffer window. Each destination has
    // its own trailing-newline counter so their collapse states stay separate.
    strid_t stream;
    int *trailing;
    if (_redirectToTranscript) {
        if (!_transcript) return;
        stream = _transcript;
        trailing = &_transcriptTrailingNewlines;
    } else {
        stream = glk_window_get_stream(_bottomWindow);
        trailing = &_trailingNewlines;
    }
    for (; *s; ++s) {
        if (*s == '\n') {
            if (*trailing >= 2)
                continue;  // already a blank line; collapse further newlines
            ++*trailing;
        } else {
            *trailing = 0;
        }
        glk_put_char_stream(stream, (unsigned char)*s);
    }
}

void Comprehend::putBottomUni(const glui32 *s) {
    strid_t stream;
    int *trailing;
    if (_redirectToTranscript) {
        if (!_transcript) return;
        stream = _transcript;
        trailing = &_transcriptTrailingNewlines;
    } else {
        stream = glk_window_get_stream(_bottomWindow);
        trailing = &_trailingNewlines;
    }
    for (; *s; ++s) {
        if (*s == '\n') {
            if (*trailing >= 2)
                continue;  // already a blank line; collapse further newlines
            ++*trailing;
        } else {
            *trailing = 0;
        }
        glk_put_char_stream_uni(stream, *s);
    }
}

void Comprehend::transcript(const char *arg) {
    // "#transcript off" stops; "#transcript [on]" starts; bare "#transcript"
    // toggles. (Mirrors the scott/taylor/plus metacommand behaviour.)
    while (*arg == ' ') ++arg;
    if (scumm_stricmp(arg, "off") == 0)
        transcriptOff();
    else if (scumm_stricmp(arg, "on") == 0)
        transcriptOn();
    else if (_transcript)
        transcriptOff();
    else
        transcriptOn();
}

void Comprehend::transcriptOn() {
    if (_transcript) {
        print("A transcript is already active.\n");
        return;
    }

    frefid_t ref = glk_fileref_create_by_prompt(
        fileusage_TextMode | fileusage_Transcript, filemode_Write, 0);
    if (!ref)
        return;

    _transcript = glk_stream_open_file_uni(ref, filemode_Write, 0);
    glk_fileref_destroy(ref);

    if (!_transcript) {
        print("Failed to create transcript file.\n");
        return;
    }

    glk_put_string_stream(_transcript, (char *)"Start of transcript\n\n");
    _transcriptTrailingNewlines = 2;  // header ended with a blank line
    print("Transcript is now on.\n");
    glk_window_set_echo_stream(_bottomWindow, _transcript);

    // Record the room the player is currently standing in, so the transcript
    // doesn't begin mid-room with no description (matches scott's behaviour).
    if (_game)
        _game->transcribeCurrentRoom();
}

void Comprehend::transcriptOff() {
    if (!_transcript) {
        print("No transcript is currently running.\n");
        return;
    }

    glk_window_set_echo_stream(_bottomWindow, nullptr);
    glk_put_string_stream(_transcript, (char *)"\n\nEnd of transcript\n");
    glk_stream_close(_transcript, nullptr);
    _transcript = nullptr;
    print("Transcript is now off.\n");
}

void Comprehend::setCentered(bool on) {
    // Toggle the centred justification style on the buffer window (used for the
    // title credits). Routed through the same stream putBottom() writes to, so
    // subsequent console_println() output is centred until switched back.
    glk_set_style_stream(glk_window_get_stream(_bottomWindow),
                         on ? style_User1 : style_Normal);
}

void Comprehend::setPreformatted(bool on) {
    // Toggle a fixed-width (monospace) style on the buffer window. Used for the
    // few game strings that are hand-drawn ASCII boxes (e.g. Talisman's
    // "departing the Persian Empire" sign): only a monospace font with the
    // original spacing preserved keeps their dash rules and '!' borders aligned.
    glk_set_style_stream(glk_window_get_stream(_bottomWindow),
                         on ? style_Preformatted : style_Normal);
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
    maybeStartHourglassFall();
    event_t ev;
    glk_request_line_event(_bottomWindow, buffer, (glui32)(maxLen - 1), 0);
    for (;;) {
        glk_select(&ev);
        if (ev.type == evtype_LineInput) break;
        if (ev.type == evtype_Timer) {
            if (_slowDrawActive) tickSlowDraw();
            if (_hourglassFalling) tickHourglass();
        } else if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
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
    maybeStartHourglassFall();
    glk_request_char_event(_bottomWindow);
    setDisableSaves(true);
    event_t ev;
    ev.type = evtype_None;
    while (ev.type != evtype_CharInput) {
        glk_select(&ev);
        if (ev.type == evtype_Timer) {
            if (_slowDrawActive) tickSlowDraw();
            if (_hourglassFalling) tickHourglass();
        } else if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
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
    // Must happen before gmSetSlowDraw/gmcgaSetSlowDraw, which would clear
    // s_recordOps and break the active-renderer check inside finishSlowDraw().
    finishSlowDraw();

    // Likewise snap any in-flight grain fall before repainting -- the new picture
    // restamps the pile at the current level, so a lingering falling grain from
    // the previous turn would be drawn over stale geometry.
    if (_hourglassFalling) {
        if (_useDhgr)
            gmDhgrCMHourglassFallAbort();
        else
            gmCMHourglassFallAbort();
        _hourglassFalling = false;
        updateTimerRequest();
    }
    // A fall deferred behind the previous reveal is stale once a new picture is
    // drawn (this picture re-arms its own); drop it so it can't fire late.
    _hourglassFallPending = false;

    // All three Talisman renderers (Apple II hi-res, Apple II double hi-res and
    // DOS CGA) can reveal a picture progressively, the way the original painted
    // the page. Record the draw order when the user has slow-draw on (and we
    // aren't replaying a transcript). Only the renderer that actually runs
    // records anything.
    //
    // _suppressSlowDraw is set by update_graphics() when it repaints a scene
    // identical to the one already on screen: there is nothing new to reveal, so
    // paint instantly rather than re-animate the same image (looks like a stutter).
    bool slow = gli_slowdraw && !gli_determinism && !_suppressSlowDraw;
    gmSetSlowDraw(slow);
    gmcgaSetSlowDraw(slow);
    gmDhgrSetSlowDraw(slow);
    gmpcjrSetSlowDraw(slow);

    // Render through the Pics opcode interpreter into the pixel buffer,
    // then blit the initial (blank or reset) state to the Glk window.
    _pics->renderPicture((int)pictureNum);
    blitSurfaceToWindow();

    // If the render queued a slow-draw reveal, start a Glk timer and let the
    // input loops (readLine / readChar) advance it tick by tick in the
    // background. The game proceeds immediately: room description is shown
    // and the prompt appears while the picture is still being revealed.
    if (gmcgaSlowDrawActive() || gmSlowDrawActive() || gmDhgrSlowDrawActive() ||
        gmpcjrSlowDrawActive()) {
        glk_request_timer_events(GM_SLOW_TICK_MS);
        _slowDrawActive = true;
    }
}

// Advance one timer tick of the background slow-draw reveal.
void Comprehend::tickSlowDraw() {
    bool more;
    int y0, y1;
    if (gmDhgrSlowDrawActive()) {
        more = gmDhgrAdvanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
        gmDhgrBlitSlowToSurface((uint32 *)_drawSurface->getPixels(),
                                _drawSurface->w, _drawSurface->h);
        if (gmDhgrSlowConsumeDirty(&y0, &y1))
            blitSurfaceRowsToWindow(y0, y1);
    } else if (gmcgaSlowDrawActive()) {
        more = gmcgaAdvanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
        gmcgaBlitSlowToSurface((uint32 *)_drawSurface->getPixels(),
                              _drawSurface->w, _drawSurface->h);
        if (gmcgaSlowConsumeDirty(&y0, &y1))
            blitSurfaceRowsToWindow(y0, y1);
    } else if (gmpcjrSlowDrawActive()) {
        more = gmpcjrAdvanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
        gmpcjrBlitSlowToSurface((uint32 *)_drawSurface->getPixels(),
                                _drawSurface->w, _drawSurface->h);
        if (gmpcjrSlowConsumeDirty(&y0, &y1))
            blitSurfaceRowsToWindow(y0, y1);
    } else {
        more = gmAdvanceSlowDraw(GM_SLOW_BYTES_PER_TICK);
        gmBlitSlowToSurface((uint32 *)_drawSurface->getPixels(),
                            _drawSurface->w, _drawSurface->h);
        if (gmSlowConsumeDirty(&y0, &y1))
            blitSurfaceRowsToWindow(y0, y1);
    }
    if (!more) {
        _slowDrawActive = false;
        // The reveal that was holding back a move-turn grain fall has finished:
        // start it now so the freed grain drops in over the freshly painted room.
        if (_hourglassFallPending && !_hourglassFalling)
            beginHourglassFall();
        updateTimerRequest();
    }
}

// Request the Glk timer iff some background animation still needs ticking.
// Both the slow-draw reveal and the grain fall share the one timer, so the last
// one to finish turns it off.
void Comprehend::updateTimerRequest() {
    glk_request_timer_events((_slowDrawActive || _hourglassFalling)
                             ? GM_SLOW_TICK_MS : 0);
}

// Start The Coveted Mirror's per-turn grain fall once the turn's graphics have
// settled (called as input is about to be read). gmDrawCMHourglass() flags a
// single-grain drop; we animate it only with the picture window open and
// slow-draw enabled, and only when no room reveal is already running (the room
// paint-in owns the timer then and the hourglass just snaps, matching the
// original's behaviour on a room change). The armed flag is consumed either way.
void Comprehend::maybeStartHourglassFall() {
    if (!gmCMHourglassConsumeFallArmed())
        return;
    if (_hourglassFalling)
        return;
    if (!_topWindow || !gli_slowdraw || gli_determinism)
        return;
    // A move repaints the room with a slow-draw reveal that owns the timer and
    // redraws the panel band each frame; running the fall at the same time would
    // fight it. Defer the fall until the reveal finishes (tickSlowDraw) so the
    // grain still drops on a move, just after the new room has painted in.
    if (_slowDrawActive) {
        _hourglassFallPending = true;
        return;
    }
    beginHourglassFall();
}

void Comprehend::beginHourglassFall() {
    _hourglassFallPending = false;
    if (_useDhgr)
        gmDhgrCMHourglassFallBegin();
    else
        gmCMHourglassFallBegin();
    _hourglassFalling = true;
    _hgTickAccum = 0;
    updateTimerRequest();
}

// Advance the grain fall, paced down from the fast slow-draw tick, and blit just
// the neck band it changed.
void Comprehend::tickHourglass() {
    if (++_hgTickAccum < HG_TICKS_PER_FRAME)
        return;
    _hgTickAccum = 0;

    int y0, y1;
    bool more;
    if (_useDhgr) {
        more = gmDhgrCMHourglassFallStep(&y0, &y1);
        gmDhgrBlitToSurface((uint32 *)_drawSurface->getPixels(),
                            _drawSurface->w, _drawSurface->h);
    } else {
        more = gmCMHourglassFallStep(&y0, &y1);
        gmBlitToSurface((uint32 *)_drawSurface->getPixels(),
                        _drawSurface->w, _drawSurface->h);
    }
    blitSurfaceRowsToWindow(y0, y1);

    if (!more) {
        _hourglassFalling = false;
        updateTimerRequest();
    }
}

// Redraw The Coveted Mirror's hourglass panel for the current sand level without
// repainting the room. update_graphics() only repaints when the room or its
// items change, so on a turn where the player stays put nothing redrew the
// draining sand -- the pile never visibly fell and the freed-grain animation was
// never armed. The original interpreter's per-turn step (cm_per_turn_graphics_step
// $42f8) updates the hourglass every turn regardless; this is the host-side
// equivalent. Re-stamping the pile via gmDrawCMHourglass() arms the fall when the
// level dropped by one (the normal per-turn drain), which the next readLine()
// animates through maybeStartHourglassFall(). The caller skips this on full-
// repaint turns, where drawPicture() already draws the hourglass (snapped).
void Comprehend::refreshCMHourglass() {
    if (!_topWindow || !_pics || !_drawSurface || !_graphicsEnabled)
        return;
    if (getGameID() != "covetedmirror")
        return;

    int sand = 0;
    if (_game)
        sand = _game->_variables[0x11];

    // Snap any still-in-flight fall from the previous turn before re-stamping the
    // pile, exactly as drawPicture() does, so a lingering grain isn't left drawn
    // over the freshly stamped geometry.
    if (_hourglassFalling) {
        if (_useDhgr)
            gmDhgrCMHourglassFallAbort();
        else
            gmCMHourglassFallAbort();
        _hourglassFalling = false;
        updateTimerRequest();
    }

    if (_useDhgr) {
        if (!gmDhgrCMPanelValid())
            return;
        gmDhgrOverlayCMPanel();
        gmDhgrDrawCMHourglass(sand);
        gmDhgrBlitToSurface((uint32 *)_drawSurface->getPixels(),
                            _drawSurface->w, _drawSurface->h);
    } else {
        if (!gmCMPanelValid())
            return;
        gmOverlayCMPanel();
        gmDrawCMHourglass(sand);
        gmBlitToSurface((uint32 *)_drawSurface->getPixels(),
                        _drawSurface->w, _drawSurface->h);
    }
    // Only the panel columns changed on the surface (the room columns still hold
    // the last-drawn room), so a full-height blit just repaints the room as-is
    // plus the updated hourglass -- simplest and cheap enough for one turn.
    blitSurfaceRowsToWindow(0, G_RENDER_HEIGHT - 1);
}

// Complete the slow-draw reveal instantly and sync _drawSurface to the final
// fully-drawn page. The caller is responsible for blitting to the window.
void Comprehend::finishSlowDraw() {
    if (!_slowDrawActive) return;
    _slowDrawActive = false;
    updateTimerRequest();
    if (gmDhgrSlowDrawActive()) {
        gmDhgrFinishSlowDraw();
        gmDhgrBlitToSurface((uint32 *)_drawSurface->getPixels(),
                            _drawSurface->w, _drawSurface->h);
    } else if (gmcgaSlowDrawActive()) {
        gmcgaFinishSlowDraw();
        gmcgaBlitToSurface((uint32 *)_drawSurface->getPixels(),
                          _drawSurface->w, _drawSurface->h);
    } else if (gmpcjrSlowDrawActive()) {
        gmpcjrFinishSlowDraw();
        gmpcjrBlitToSurface((uint32 *)_drawSurface->getPixels(),
                            _drawSurface->w, _drawSurface->h);
    } else {
        gmFinishSlowDraw();
        gmBlitToSurface((uint32 *)_drawSurface->getPixels(),
                        _drawSurface->w, _drawSurface->h);
    }
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

void Comprehend::hideGraphics() {
    // The status window stays where showGraphics() reopened it (between the
    // picture and the buffer); only the picture window goes away, leaving a
    // text+status layout.
    if (_topWindow) {
        glk_window_close(_topWindow, nullptr);
        _topWindow = nullptr;
    }
    _graphicsEnabled = false;
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
    // Graphics windows default to a white background (glkimp window.c), which
    // shows through in the centring margins and as thin slivers where the scaled
    // fill-rects don't perfectly tile. Match it to the buffer's Normal-style
    // background instead, so the picture window blends with the rest of the
    // screen (as the Level9 and Magnetic interpreters do). Glk offers no direct
    // way to read a window's background, so probe the style hint and fall back to
    // white if the front-end can't report it.
    if (_topWindow) {
        glui32 background;
        if (!glk_style_measure(_bottomWindow, style_Normal,
                               stylehint_BackColor, &background))
            background = 0x00ffffff;
        glk_window_set_background_color(_topWindow, background);
        glk_window_clear(_topWindow);
    }
    // Reopen status above _bottomWindow — inserts it between graphics and buffer.
    // Height is set to fit the room description on the next printRoomDesc().
    _statusWindow = glk_window_open(_bottomWindow,
        winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, GLK_STATUS_ROCK);
}

void Comprehend::setDhgrMode(bool on) {
    if (on == _useDhgr)
        return;
    _useDhgr = on;
    // Widen/narrow the persistent render surface. This clears it; the caller
    // (the toggle / picture code) redraws the current picture afterwards.
    if (_drawSurface)
        _drawSurface->setRenderWidth(on ? G_RENDER_WIDTH_DHGR : G_RENDER_WIDTH);
    // The horizontal step changed, so the integer scale may need to (re)round to
    // even; repaint whatever is currently shown.
    if (_topWindow) {
        recomputeGraphicsScale();
        glk_window_clear(_topWindow);
    }
}

void Comprehend::setPcjrMode(bool on) {
    if (on == _usePcjr)
        return;
    _usePcjr = on;
    // Same 280x160 logical surface as the CGA renderer, so no resize is needed;
    // just clear the window so the toggle/picture code repaints in the new mode.
    if (_topWindow)
        glk_window_clear(_topWindow);
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
    // The physical width is the same in both hi-res modes: standard is
    // G_RENDER_WIDTH*_pixelSize; DHGR is its 560 columns at half-width
    // (560 * _pixelSize/2 == 280 * _pixelSize), so the 280-unit centring holds.
    int xOff = ((int)winW - G_RENDER_WIDTH * _pixelSize) / 2;
    if (xOff < 0) xOff = 0;
    const int surfaceW = _drawSurface->w;   // 280 (std) or 560 (DHGR)
    const int stepX = pixelStepX();          // _pixelSize (std) or _pixelSize/2 (DHGR)

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
        while (x < surfaceW) {
            uint32 c = surfaceColorAt(_drawSurface, x, y);
            int x0 = x;
            do { ++x; } while (x < surfaceW && surfaceColorAt(_drawSurface, x, y) == c);
            if ((c & 0xff) == 0)
                continue;   // transparent — leave the background untouched
            // Strip alpha and map to Glk's 0xRRGGBB.
            glui32 rgb = (glui32)((c >> 8) & 0xffffff);
            glk_window_fill_rect(_topWindow, rgb,
                xOff + x0 * stepX,
                y * _pixelSize,
                (x - x0) * stepX,
                _pixelSize);
        }
    }

    // The Coveted Mirror's room (a bright wall) butts directly against the black
    // hourglass panel at column 24. The Apple II NTSC artifact renderer extends a
    // white run one pixel past its last column, so the wall's white bleeds into
    // the panel's leftmost pixel — a stray white fringe down the panel's left
    // edge. The page is correct (the panel byte is black); only the rendered
    // colour bleeds, so we can't fix it on the page. Repaint the panel's two
    // leftmost (always-black, logo starts at column 26) columns in black on top.
    // CM standard hi-res only; DHGR has no panel. gmCMPanelValid() gates this to
    // when the panel is actually present (false on the title screen, whose
    // columns 24-25 are real picture content, not panel).
    if (!_useDhgr && getGameID() == "covetedmirror" && gmCMPanelValid()) {
        const int kCMPanelCol = 24;            // pics.cpp gmCaptureCMPanel(24, 39)
        int px0 = xOff + (kCMPanelCol * 7) * stepX;
        int w = 2 * 7 * stepX;                 // columns 24-25
        glk_window_fill_rect(_topWindow, 0x000000,
            px0, y0 * _pixelSize, w, (y1 - y0 + 1) * _pixelSize);
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
    // Double hi-res halves the horizontal step (pixelStepX == scale/2), so the
    // vertical scale must be even (and >= 2) for the 560 columns to land on
    // whole window pixels.
    if (_useDhgr) {
        if (scale < 2) scale = 2;
        scale &= ~1;
    }
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

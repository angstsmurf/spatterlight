/*
 * Compatibility shim for porting the ScummVM Comprehend interpreter to
 * Spatterlight. Same idea as terps/archetype/archetype_compat.h, but with
 * the extra Graphics:: and Common:: types Comprehend needs:
 *  - Graphics::ManagedSurface (pixel buffer with line/fill primitives)
 *  - Graphics::PixelFormat
 *  - Common::Rect / Common::Point
 *  - Common::U32String (Unicode strings)
 *  - Common::Serializer (save/load sync)
 *  - Common::Archive (stubbed — we render+blit instead)
 *  - Common::StringArray = Common::Array<Common::String>
 *
 * The drawing pipeline follows scott's apple2_vector_draw approach:
 * render the picture into a 32-bpp pixel buffer, then blit it to a Glk
 * Graphics window with glk_window_fill_rect() per pixel/run.
 */

#ifndef COMPREHEND_COMPAT_H
#define COMPREHEND_COMPAT_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <utility>
#include <memory>

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef unsigned int uint;
typedef uint8_t  byte;

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef ABS
#define ABS(x) (((x) < 0) ? -(x) : (x))
#endif
#ifndef CLIP
#define CLIP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#endif
#ifndef SWAP
#define SWAP(a, b) do { auto __t = (a); (a) = (b); (b) = __t; } while (0)
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define MSVC_PRINTF
#define GCC_PRINTF(a,b) __attribute__((format(printf, a, b)))

void comprehend_error_impl(const char *fmt, ...) GCC_PRINTF(1, 2);
void comprehend_debug_impl(const char *fmt, ...) GCC_PRINTF(1, 2);
void comprehend_debug_level_impl(int level, const char *fmt, ...) GCC_PRINTF(2, 3);
void comprehend_debugN_impl(const char *fmt, ...) GCC_PRINTF(1, 2);
void comprehend_debugC_impl(int chan, const char *fmt, ...);
void comprehend_warning_impl(const char *fmt, ...) GCC_PRINTF(1, 2);

#define error(...) comprehend_error_impl(__VA_ARGS__)

// ScummVM's debug() and debugN() accept an optional leading int level; both
// forms exist in the codebase so we route through a single variadic macro.
#define debug(...) comprehend_debug_impl(__VA_ARGS__)
#define debugN(...) comprehend_debugN_impl(__VA_ARGS__)
#define debugC(...) comprehend_debugC_impl(__VA_ARGS__)
#define debugCN(...) comprehend_debugC_impl(__VA_ARGS__)
#define warning(...) comprehend_warning_impl(__VA_ARGS__)

// Debug channel constants the original references; values are arbitrary
// since DebugMan stubs always report disabled.
enum {
    kDebugScripts = 1 << 0,
    kDebugGraphics = 1 << 1,
    kDebugLevelScripts = 1 << 2
};

// ScummVM uses _("text") for i18n. Spatterlight has no translation layer,
// so just pass through.
#define _(s) (s)

// ScummVM provides scumm_stricmp; alias to platform's strcasecmp.
inline int scumm_stricmp(const char *a, const char *b) {
    while (*a && *b) {
        int da = std::tolower((unsigned char)*a);
        int db = std::tolower((unsigned char)*b);
        if (da != db) return da - db;
        ++a; ++b;
    }
    return (int)(unsigned char)std::tolower((unsigned char)*a)
         - (int)(unsigned char)std::tolower((unsigned char)*b);
}

// ScummVM provides scumm_strnicmp; case-insensitive compare of the first n chars.
inline int scumm_strnicmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        int da = std::tolower((unsigned char)a[i]);
        int db = std::tolower((unsigned char)b[i]);
        if (da != db) return da - db;
        if (!a[i]) break;
    }
    return 0;
}

namespace Common {

class String : public std::string {
public:
    String() {}
    String(const char *s) : std::string(s ? s : "") {}
    String(const char *s, size_t len) : std::string(s, len) {}
    String(const char *b, const char *e) : std::string(b, e) {}
    String(const std::string &s) : std::string(s) {}
    explicit String(char c) : std::string(1, c) {}

    String &operator=(const char *s) { assign(s ? s : ""); return *this; }
    String &operator=(char c) { assign(1, c); return *this; }
    String &operator=(const std::string &s) { assign(s); return *this; }

    static String format(const char *fmt, ...) GCC_PRINTF(1, 2);
    static String vformat(const char *fmt, va_list args);

    char lastChar() const { return empty() ? 0 : (*this)[size() - 1]; }
    char firstChar() const { return empty() ? 0 : (*this)[0]; }

    bool hasPrefix(const char *pre) const {
        size_t n = std::strlen(pre);
        return size() >= n && std::memcmp(data(), pre, n) == 0;
    }
    bool hasPrefix(const String &pre) const { return hasPrefix(pre.c_str()); }
    bool hasPrefixIgnoreCase(const char *pre) const;
    bool hasSuffix(const char *suf) const {
        size_t n = std::strlen(suf);
        return size() >= n && std::memcmp(data() + size() - n, suf, n) == 0;
    }
    bool hasSuffixIgnoreCase(const char *suf) const;

    bool contains(const char *s) const { return find(s) != std::string::npos; }
    bool contains(const String &s) const { return find(s) != std::string::npos; }
    bool contains(char c) const { return find(c) != std::string::npos; }

    bool equalsIgnoreCase(const char *other) const {
        return scumm_stricmp(c_str(), other ? other : "") == 0;
    }
    bool equalsIgnoreCase(const String &other) const {
        return scumm_stricmp(c_str(), other.c_str()) == 0;
    }

    size_t findFirstOf(char c, size_t start = 0) const {
        size_t p = find(c, start);
        return p == std::string::npos ? std::string::npos : p;
    }

    void deleteLastChar() { if (!empty()) pop_back(); }
    void deleteChar(size_t i) { if (i < size()) erase(i, 1); }
    void toLowercase();
    void toUppercase();

    // Used by Comprehend::printRoomDesc().
    void wordWrap(size_t maxLineWidth);
};

inline bool isAlpha(char c) { return std::isalpha((unsigned char)c) != 0; }
inline bool isDigit(char c) { return std::isdigit((unsigned char)c) != 0; }
inline bool isAlnum(char c) { return std::isalnum((unsigned char)c) != 0; }
inline bool isSpace(char c) { return std::isspace((unsigned char)c) != 0; }
inline bool isPrint(char c) { return std::isprint((unsigned char)c) != 0; }

// ScummVM's strcpy_s deduces destination size from the array; we mirror it.
template<size_t N>
inline void strcpy_s(char (&dst)[N], const char *src) {
    if (!src) { dst[0] = 0; return; }
    std::strncpy(dst, src, N - 1);
    dst[N - 1] = 0;
}

// A 32-bit Unicode string. Used only for a handful of formatted messages in
// Comprehend; treat it as a vector of codepoints. We provide just the API
// surface the engine touches.
class U32String : public std::vector<uint32> {
public:
    U32String() {}
    // Made explicit so Comprehend::print(const char*, ...) wins overload
    // resolution against Comprehend::print(const U32String &, ...).
    explicit U32String(const char *s) {
        while (s && *s) push_back((uint32)(unsigned char)*s++);
    }
    explicit U32String(const String &s) {
        for (char c : s) push_back((uint32)(unsigned char)c);
    }

    const uint32 *u32_str() const { return empty() ? nullptr : data(); }
    const uint32 *begin() const { return data(); }
    const uint32 *end() const { return data() + size(); }

    static void vformat(const uint32 *fmtBegin, const uint32 *fmtEnd,
                        U32String &out, va_list args);
};

template<typename T>
class Array : public std::vector<T> {
public:
    using std::vector<T>::vector;
    Array() {}
};

typedef Array<String> StringArray;

// Minimal sort/fill/copy used by file_buf.cpp etc.
template<typename It, typename V>
void fill(It b, It e, const V &v) { std::fill(b, e, v); }
template<typename It, typename Out>
Out copy(It b, It e, Out o) { return std::copy(b, e, o); }

template<class T>
constexpr T&& forward(typename std::remove_reference<T>::type& v) noexcept {
    return static_cast<T&&>(v);
}
template<class T>
constexpr T&& forward(typename std::remove_reference<T>::type&& v) noexcept {
    return static_cast<T&&>(v);
}

// --- Stream abstraction (mirrors archetype's; extended slightly) ---

class Stream {
public:
    virtual ~Stream() {}
};

class ReadStream : public virtual Stream {
public:
    // Default-implemented so derivers that only override read() (e.g.
    // FileBuffer) get readByte() for free, matching ScummVM's behaviour.
    virtual byte readByte() {
        byte b = 0;
        if (read(&b, 1) != 1) return 0;
        return b;
    }
    virtual int8 readSByte() { return (int8)readByte(); }
    virtual uint16 readUint16LE() {
        byte lo = readByte();
        byte hi = readByte();
        return (uint16)(lo | (hi << 8));
    }
    virtual int16 readSint16LE() { return (int16)readUint16LE(); }
    virtual uint32 readUint32LE() {
        uint32 lo = readUint16LE();
        uint32 hi = readUint16LE();
        return lo | (hi << 16);
    }
    virtual int32 readSint32LE() { return (int32)readUint32LE(); }
    virtual uint16 readUint16BE() {
        byte hi = readByte();
        byte lo = readByte();
        return (uint16)((hi << 8) | lo);
    }
    virtual uint32 readUint32BE() {
        uint32 hi = readUint16BE();
        uint32 lo = readUint16BE();
        return (hi << 16) | lo;
    }
    virtual uint32 read(void *buf, uint32 size) = 0;
    virtual bool eos() const = 0;
};

class WriteStream : public virtual Stream {
public:
    virtual void writeByte(byte b) = 0;
    virtual void writeSByte(int8 b) { writeByte((byte)b); }
    virtual void writeUint16LE(uint16 v) {
        writeByte((byte)(v & 0xff));
        writeByte((byte)((v >> 8) & 0xff));
    }
    virtual void writeSint16LE(int16 v) { writeUint16LE((uint16)v); }
    virtual void writeUint32LE(uint32 v) {
        writeUint16LE((uint16)(v & 0xffff));
        writeUint16LE((uint16)((v >> 16) & 0xffff));
    }
    virtual void writeSint32LE(int32 v) { writeUint32LE((uint32)v); }
    virtual uint32 write(const void *buf, uint32 size) = 0;
};

class SeekableReadStream : public ReadStream {
public:
    virtual int64 pos() const = 0;
    virtual int64 size() const = 0;
    virtual bool seek(int64 offset, int whence = SEEK_SET) = 0;
    virtual bool skip(uint32 offset) { return seek((int64)offset, SEEK_CUR); }
};

class FileStream : public SeekableReadStream, public WriteStream {
public:
    FileStream() : _f(nullptr), _eos(false), _inMem(false), _memPos(0) {}
    ~FileStream() override { close(); }

    bool open(const String &path, const char *mode = "rb");
    void close();
    bool isOpen() const { return _f != nullptr || _inMem; }

    byte readByte() override;
    uint32 read(void *buf, uint32 size) override;
    bool eos() const override { return _eos; }

    int64 pos() const override;
    int64 size() const override;
    bool seek(int64 offset, int whence = SEEK_SET) override;

    void writeByte(byte b) override;
    uint32 write(const void *buf, uint32 size) override;

    // ScummVM has static Common::File::exists(path).
    static bool exists(const String &path);

private:
    FILE *_f;
    bool _eos;

    // When the game data comes from an Apple II disk image, files are served
    // from an in-memory virtual filesystem (see DiskImageFS below) instead of
    // the real filesystem.
    bool _inMem;
    std::vector<byte> _memBuf;
    size_t _memPos;
};

// In-memory virtual filesystem populated by extracting files from an Apple II
// disk image (apple2.cpp). Keyed by lower-cased basename, so the engine's
// case-mixed FileBuffer("g0") / File::open("RA") lookups all resolve. When
// empty (the normal DOS/PC case) every file access falls through to disk.
//
// Each loaded disk side is kept as its own file set. Most games are a single
// dataset spread over several sides, so lookups fall back across all disks.
// Multi-disk games (Crimson Crown, Talisman) instead carry a *different*
// dataset per disk under colliding names (g0, RA, ...), and switch the active
// disk at runtime via selectDiskByGameMagic(); the active disk is always
// consulted first.
namespace DiskImageFS {
    void beginDisk();   // start a new disk; subsequent add()s go to it
    void add(const String &name, const byte *data, size_t len);

    // Make the disk whose "g0" begins with `magic` the active one. Returns
    // false if no such disk is loaded.
    bool selectDiskByGameMagic(uint16 magic);

    bool has(const String &name);
    const std::vector<byte> *get(const String &name);
    bool active();   // true once any file has been added
    void clear();
}

// Used by charset.cpp for picking the right font baseline. ScummVM's md5.h
// hashes a stream; we hash whatever bytes we can read up to `length`.
String computeStreamMD5AsString(ReadStream *stream, uint32 length);
String computeStreamMD5AsString(ReadStream &stream, uint32 length);

typedef FileStream File;

class Path {
public:
    Path() {}
    Path(const String &s) : _s(s) {}
    Path(const char *s) : _s(s ? s : "") {}
    const String &toString() const { return _s; }
    String baseName() const;
    operator const String&() const { return _s; }
private:
    String _s;
};

// In-memory R/W stream used by pics.cpp (kept for compile-compat; we no
// longer actually drive Glk image registration through it).
enum DisposeAfterUse {
    NO = 0,
    YES = 1
};

class MemoryReadWriteStream : public SeekableReadStream, public WriteStream {
public:
    MemoryReadWriteStream(DisposeAfterUse) : _pos(0) {}
    byte readByte() override {
        if (_pos >= (int64)_buf.size()) { _eos = true; return 0; }
        return _buf[_pos++];
    }
    uint32 read(void *p, uint32 n) override {
        uint32 avail = (uint32)((int64)_buf.size() - _pos);
        uint32 got = MIN(n, avail);
        std::memcpy(p, _buf.data() + _pos, got);
        _pos += got;
        if (got < n) _eos = true;
        return got;
    }
    bool eos() const override { return _eos; }
    int64 pos() const override { return _pos; }
    int64 size() const override { return (int64)_buf.size(); }
    bool seek(int64 o, int w) override {
        switch (w) {
            case SEEK_SET: _pos = o; break;
            case SEEK_CUR: _pos += o; break;
            case SEEK_END: _pos = (int64)_buf.size() + o; break;
        }
        _eos = false;
        return true;
    }
    void writeByte(byte b) override { _buf.push_back(b); }
    // Push the little-endian bytes straight into the buffer instead of routing
    // each through the virtual writeByte (the base class's default). The undo
    // snapshot serializes thousands of uint16 fields per turn through here.
    void writeUint16LE(uint16 v) override {
        _buf.push_back((byte)(v & 0xff));
        _buf.push_back((byte)((v >> 8) & 0xff));
    }
    void writeUint32LE(uint32 v) override {
        _buf.push_back((byte)(v & 0xff));
        _buf.push_back((byte)((v >> 8) & 0xff));
        _buf.push_back((byte)((v >> 16) & 0xff));
        _buf.push_back((byte)((v >> 24) & 0xff));
    }
    uint32 write(const void *p, uint32 n) override {
        const byte *bp = (const byte *)p;
        _buf.insert(_buf.end(), bp, bp + n);
        return n;
    }
    // Pre-size the backing buffer so a run of writeByte()s (e.g. serializing a
    // save/undo snapshot) doesn't repeatedly reallocate and grow.
    void reserve(size_t n) { _buf.reserve(n); }
    const byte *getData() const { return _buf.data(); }
private:
    std::vector<byte> _buf;
    int64 _pos = 0;
    bool _eos = false;
};

enum ErrorCode {
    kNoError = 0,
    kNoGameDataFoundError
};

class Error {
public:
    Error(ErrorCode c = kNoError) : _code(c) {}
    ErrorCode getCode() const { return _code; }
private:
    ErrorCode _code;
};

// Save/load synchroniser. Compatible enough with ScummVM's Common::Serializer
// for ComprehendGame::synchronizeSave to compile; we hand it the relevant
// read or write stream depending on direction. Templated so the engine can
// pass int / uint / size_t / size_type without explicit casts (ScummVM's
// Serializer accepts any integral type by reference).
class Serializer {
public:
    Serializer(SeekableReadStream *in, WriteStream *out) : _in(in), _out(out) {}
    bool isSaving() const { return _out != nullptr; }
    bool isLoading() const { return _in != nullptr; }

    template<typename T>
    void syncAsByte(T &v, uint8 = 0) {
        if (_in) v = (T)_in->readByte();
        else _out->writeByte((byte)v);
    }
    template<typename T>
    void syncAsUint16LE(T &v, uint8 = 0) {
        if (_in) v = (T)_in->readUint16LE();
        else _out->writeUint16LE((uint16)v);
    }
    template<typename T>
    void syncAsSint16LE(T &v, uint8 = 0) {
        if (_in) v = (T)_in->readSint16LE();
        else _out->writeSint16LE((int16)v);
    }
    template<typename T>
    void syncAsUint32LE(T &v, uint8 = 0) {
        if (_in) v = (T)_in->readUint32LE();
        else _out->writeUint32LE((uint32)v);
    }
    void syncBytes(byte *p, uint32 n, uint8 = 0) {
        if (_in) _in->read(p, n);
        else _out->write(p, n);
    }
private:
    SeekableReadStream *_in;
    WriteStream *_out;
};

// Archive stub. Comprehend's Pics inherits this so the engine can stick
// it into SearchMan and have ScummVM's glk image loader pull pixel data
// through createReadStreamForMember(). On Spatterlight we render+blit
// directly, so the interface only needs to compile.
class Path;
class ArchiveMember {
public:
    virtual ~ArchiveMember() {}
    virtual String getName() const = 0;
};
typedef std::shared_ptr<ArchiveMember> ArchiveMemberPtr;
typedef std::vector<ArchiveMemberPtr> ArchiveMemberList;

class Archive {
public:
    virtual ~Archive() {}
    virtual bool hasFile(const Path &path) const = 0;
    virtual int listMembers(ArchiveMemberList &list) const = 0;
    virtual const ArchiveMemberPtr getMember(const Path &path) const = 0;
    virtual SeekableReadStream *createReadStreamForMember(const Path &path) const = 0;
};

class GenericArchiveMember : public ArchiveMember {
public:
    GenericArchiveMember(const Path &p, const Archive &) : _name(p.toString()) {}
    String getName() const override { return _name; }
private:
    String _name;
};

class SearchManager {
public:
    void add(const String &, Archive *, int = 0, bool autoFree = true) { (void)autoFree; }
    void remove(const String &) {}
};
extern SearchManager SearchMan;

class ConfigManager {
public:
    bool hasKey(const char *) { return false; }
    int getInt(const char *) { return -1; }
};
extern ConfigManager ConfMan;

class DebugManager {
public:
    void addDebugChannel(int, const char *, const char *) {}
    bool isDebugChannelEnabled(int) const { return false; }
    void enableDebugChannel(int) {}
    void disableDebugChannel(int) {}
};
extern DebugManager DebugMan;

} // namespace Common

using Common::DebugMan;
using Common::ConfMan;
using Common::SearchMan;

struct TimeDate {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
};
struct OSystem {
    void getTimeAndDate(TimeDate &td);
};
extern OSystem *g_system;

// --- Graphics:: namespace ---

namespace Common {

template<typename T>
struct PointT { T x, y; PointT() : x(0), y(0) {} PointT(T xx, T yy) : x(xx), y(yy) {} };
typedef PointT<int16> Point;

struct Rect {
    int16 left, top, right, bottom;
    Rect() : left(0), top(0), right(0), bottom(0) {}
    Rect(int16 r, int16 b) : left(0), top(0), right(r), bottom(b) {}
    Rect(int16 l, int16 t, int16 r, int16 b) : left(l), top(t), right(r), bottom(b) {}
    int16 width() const { return right - left; }
    int16 height() const { return bottom - top; }
};

} // namespace Common

namespace Graphics {

// Minimal pixel-format descriptor. Comprehend uses one (32bpp ARGB);
// we just need to remember the layout for colorToRGB().
struct PixelFormat {
    uint8 bytesPerPixel;
    uint8 rBits, gBits, bBits, aBits;
    uint8 rShift, gShift, bShift, aShift;
    PixelFormat() : bytesPerPixel(0), rBits(0), gBits(0), bBits(0), aBits(0),
                    rShift(0), gShift(0), bShift(0), aShift(0) {}
    PixelFormat(uint8 bpp,
                uint8 rb, uint8 gb, uint8 bb, uint8 ab,
                uint8 rs, uint8 gs, uint8 bs, uint8 as)
        : bytesPerPixel(bpp), rBits(rb), gBits(gb), bBits(bb), aBits(ab),
          rShift(rs), gShift(gs), bShift(bs), aShift(as) {}

    void colorToRGB(uint32 color, byte &r, byte &g, byte &b) const {
        // For Comprehend's 32-bit ARGB (RGB << 8 | A): bytesPerPixel=4,
        // rShift=24, gShift=16, bShift=8.
        r = (byte)((color >> rShift) & ((1 << rBits) - 1));
        g = (byte)((color >> gShift) & ((1 << gBits) - 1));
        b = (byte)((color >> bShift) & ((1 << bBits) - 1));
    }
};

// 32-bpp RGBA pixel buffer with the primitives Comprehend's Surface needs.
// Pixels are stored as uint32 in whatever endianness the host uses; the
// engine's RGB(r,g,b) macro packs (r<<24)|(g<<16)|(b<<8)|0xff.
class ManagedSurface {
public:
    int w = 0;
    int h = 0;
    int pitch = 0;  // bytes per row
    PixelFormat format;

    ManagedSurface() {}
    ManagedSurface(int width, int height, const PixelFormat &fmt) { create(width, height, fmt); }
    ~ManagedSurface() { delete[] _pixels; }

    void create(int width, int height, const PixelFormat &fmt) {
        delete[] _pixels;
        w = width;
        h = height;
        format = fmt;
        pitch = w * (fmt.bytesPerPixel ? fmt.bytesPerPixel : 4);
        _pixels = new uint32[w * h]();
    }

    void *getPixels() { return _pixels; }
    const void *getPixels() const { return _pixels; }
    void *getBasePtr(int x, int y) { return _pixels + y * w + x; }
    const void *getBasePtr(int x, int y) const { return _pixels + y * w + x; }

    void clear(uint32 color = 0) {
        for (int i = 0; i < w * h; ++i) _pixels[i] = color;
    }
    void fillRect(const Common::Rect &r, uint32 color) {
        for (int y = r.top; y < r.bottom; ++y) {
            if (y < 0 || y >= h) continue;
            for (int x = r.left; x < r.right; ++x) {
                if (x < 0 || x >= w) continue;
                _pixels[y * w + x] = color;
            }
        }
    }
    void frameRect(const Common::Rect &r, uint32 color) {
        for (int x = r.left; x < r.right; ++x) {
            if (x < 0 || x >= w) continue;
            if (r.top    >= 0 && r.top    < h) _pixels[r.top    * w + x] = color;
            if (r.bottom-1 >= 0 && r.bottom-1 < h) _pixels[(r.bottom-1) * w + x] = color;
        }
        for (int y = r.top; y < r.bottom; ++y) {
            if (y < 0 || y >= h) continue;
            if (r.left    >= 0 && r.left    < w) _pixels[y * w + r.left]    = color;
            if (r.right-1 >= 0 && r.right-1 < w) _pixels[y * w + r.right-1] = color;
        }
    }
    void drawLine(int x1, int y1, int x2, int y2, uint32 color);

private:
    uint32 *_pixels = nullptr;
};

// Graphics::Surface is the lighter base in ScummVM (ManagedSurface owns the
// pixels, Surface is just a non-owning view). We collapse both into
// ManagedSurface since Comprehend never relies on the distinction.
typedef ManagedSurface Surface;

// Base font used by Comprehend's CharSet / TalismanFont. Mirrors the subset
// of Graphics::Font the engine touches.
class Font {
public:
    virtual ~Font() {}
    virtual int getFontHeight() const { return 8; }
    virtual int getMaxCharWidth() const { return 8; }
    virtual int getCharWidth(uint32 chr) const { (void)chr; return 8; }
    virtual int getKerningOffset(uint32 left, uint32 right) const {
        (void)left; (void)right; return 0;
    }
    virtual Common::Rect getBoundingBox(uint32 chr) const {
        (void)chr; return Common::Rect(0, 0, 8, 8);
    }
    virtual void drawChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const {
        (void)dst; (void)chr; (void)x; (void)y; (void)color;
    }
};

} // namespace Graphics

#endif // COMPREHEND_COMPAT_H

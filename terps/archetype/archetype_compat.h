/*
 * Compatibility shim for porting the ScummVM Archetype interpreter to
 * Spatterlight. Provides minimal stand-ins for the ScummVM Common::*
 * types, the engine framework, debug/error macros, and a tiny stream
 * abstraction over FILE*. The goal is to keep the original interpreter
 * sources almost untouched.
 */

#ifndef ARCHETYPE_COMPAT_H
#define ARCHETYPE_COMPAT_H

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

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
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

#define MSVC_PRINTF
#define GCC_PRINTF(a,b) __attribute__((format(printf, a, b)))

void archetype_error_impl(const char *fmt, ...) GCC_PRINTF(1, 2);
void archetype_debug_impl(const char *fmt, ...) GCC_PRINTF(1, 2);
void archetype_debugN_impl(const char *fmt, ...) GCC_PRINTF(1, 2);

#define error(...) archetype_error_impl(__VA_ARGS__)
#define debug(...) archetype_debug_impl(__VA_ARGS__)
#define debugN(...) archetype_debugN_impl(__VA_ARGS__)

namespace Common {

class String : public std::string {
public:
    String() {}
    String(const char *s) : std::string(s ? s : "") {}
    String(const char *s, size_t len) : std::string(s, len) {}
    String(const char *b, const char *e) : std::string(b, e) {}
    String(const std::string &s) : std::string(s) {}
    explicit String(char c) : std::string(1, c) {}

    // operator= is not inherited from std::string; provide explicit forwards
    // so the derived Archetype::String can call us via Common::String::operator=.
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
    bool contains(const char *s) const { return find(s) != std::string::npos; }
    bool contains(const String &s) const { return find(s) != std::string::npos; }
    bool contains(char c) const { return find(c) != std::string::npos; }

    void deleteLastChar() { if (!empty()) pop_back(); }
    void deleteChar(size_t i) { if (i < size()) erase(i, 1); }
    void toLowercase();
};

inline bool isAlpha(char c) { return std::isalpha((unsigned char)c) != 0; }
inline bool isDigit(char c) { return std::isdigit((unsigned char)c) != 0; }
inline bool isAlnum(char c) { return std::isalnum((unsigned char)c) != 0; }

template<typename T>
class Array : public std::vector<T> {
public:
    Array() {}
};

// Mirrors std::forward; the Archetype interpreter calls Common::forward<T>(v)
// via templated write/writeln helpers.
template<class T>
constexpr T&& forward(typename std::remove_reference<T>::type& v) noexcept {
    return static_cast<T&&>(v);
}
template<class T>
constexpr T&& forward(typename std::remove_reference<T>::type&& v) noexcept {
    return static_cast<T&&>(v);
}

// Tiny stream abstraction. ScummVM's Common::Stream has many readers/writers
// for little-endian primitives; we provide just the ones the interpreter
// actually calls. ReadStream/WriteStream are virtual so saveload.cpp's
// dynamic_cast<> works.

class Stream {
public:
    virtual ~Stream() {}
};

class ReadStream : public virtual Stream {
public:
    virtual byte readByte() = 0;
    virtual int8 readSByte() {
        byte b = readByte();
        return (int8)b;
    }
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
    virtual int32 pos() const = 0;
    virtual int32 size() const = 0;
    virtual bool seek(int32 offset, int whence = SEEK_SET) = 0;
};

class FileStream : public SeekableReadStream, public WriteStream {
public:
    FileStream() : _f(nullptr), _eos(false) {}
    ~FileStream() override { close(); }

    bool open(const String &path, const char *mode = "rb");
    void close();
    bool isOpen() const { return _f != nullptr; }

    byte readByte() override;
    uint32 read(void *buf, uint32 size) override;
    bool eos() const override { return _eos; }

    int32 pos() const override;
    int32 size() const override;
    bool seek(int32 offset, int whence) override;

    void writeByte(byte b) override;
    uint32 write(const void *buf, uint32 size) override;

private:
    FILE *_f;
    bool _eos;
};

// In ScummVM, Common::File works with a Common::Path. We collapse both into
// FileStream so the interpreter doesn't have to know the difference.
typedef FileStream File;

class Path {
public:
    Path() {}
    Path(const String &s) : _s(s) {}
    Path(const char *s) : _s(s ? s : "") {}
    const String &toString() const { return _s; }
    operator const String&() const { return _s; }
private:
    String _s;
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

// Stub config & debug singletons. The Archetype interpreter uses these for
// optional debug output and for the ScummVM launcher's save-slot integration;
// neither applies on Spatterlight.
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

// In ScummVM these singletons are referenced as bare DebugMan / ConfMan,
// not Common::DebugMan; expose them at global scope to match.
using Common::DebugMan;
using Common::ConfMan;

// ScummVM exposes the host clock via OSystem::getTimeAndDate(TimeDate&).
// For Spatterlight we just wrap localtime().
struct TimeDate {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
};
struct OSystem {
    void getTimeAndDate(TimeDate &td);
};
extern OSystem *g_system;

#endif // ARCHETYPE_COMPAT_H

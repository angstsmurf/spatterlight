/*
 * Implementations for the Common::* shims declared in archetype_compat.h.
 */

#include "archetype_compat.h"

extern "C" {
#include "glk.h"
}

namespace Common {

ConfigManager ConfMan;
DebugManager DebugMan;

String String::vformat(const char *fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int needed = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return String();
    std::string out;
    out.resize((size_t)needed);
    std::vsnprintf(&out[0], (size_t)needed + 1, fmt, args);
    return String(out);
}

String String::format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String r = vformat(fmt, args);
    va_end(args);
    return r;
}

void String::toLowercase() {
    for (size_t i = 0; i < size(); ++i)
        (*this)[i] = (char)std::tolower((unsigned char)(*this)[i]);
}

bool FileStream::open(const String &path, const char *mode) {
    close();
    _f = std::fopen(path.c_str(), mode);
    _eos = false;
    return _f != nullptr;
}

void FileStream::close() {
    if (_f) {
        std::fclose(_f);
        _f = nullptr;
    }
    _eos = false;
}

byte FileStream::readByte() {
    int c = _f ? std::fgetc(_f) : EOF;
    if (c == EOF) {
        _eos = true;
        return 0;
    }
    return (byte)c;
}

uint32 FileStream::read(void *buf, uint32 size) {
    if (!_f) return 0;
    size_t n = std::fread(buf, 1, size, _f);
    if (n < size) _eos = true;
    return (uint32)n;
}

int32 FileStream::pos() const {
    return _f ? (int32)std::ftell(_f) : -1;
}

int32 FileStream::size() const {
    if (!_f) return -1;
    long cur = std::ftell(_f);
    std::fseek(_f, 0, SEEK_END);
    long end = std::ftell(_f);
    std::fseek(_f, cur, SEEK_SET);
    return (int32)end;
}

bool FileStream::seek(int32 offset, int whence) {
    if (!_f) return false;
    _eos = false;
    return std::fseek(_f, offset, whence) == 0;
}

void FileStream::writeByte(byte b) {
    if (_f) std::fputc(b, _f);
}

uint32 FileStream::write(const void *buf, uint32 size) {
    if (!_f) return 0;
    return (uint32)std::fwrite(buf, 1, size, _f);
}

} // namespace Common

static OSystem s_system;
OSystem *g_system = &s_system;

void OSystem::getTimeAndDate(TimeDate &td) {
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    if (lt) {
        td.tm_sec  = lt->tm_sec;
        td.tm_min  = lt->tm_min;
        td.tm_hour = lt->tm_hour;
        td.tm_mday = lt->tm_mday;
        td.tm_mon  = lt->tm_mon + 1;
        td.tm_year = lt->tm_year + 1900;
    } else {
        td = TimeDate{};
    }
}

// Send error/debug output through the Glk text window so the player sees
// problems instead of stderr (which a windowed Glk app may not surface).
static void put_va(const char *prefix, const char *fmt, va_list ap) {
    char buf[1024];
    if (prefix && *prefix)
        std::snprintf(buf, sizeof(buf), "%s", prefix);
    size_t off = std::strlen(buf);
    std::vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
    size_t len = std::strlen(buf);
    if (len > 0 && buf[len - 1] != '\n') {
        if (len + 1 < sizeof(buf)) {
            buf[len] = '\n';
            buf[len + 1] = '\0';
        }
    }
    glk_put_string(buf);
}

void archetype_error_impl(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    put_va("Error: ", fmt, ap);
    va_end(ap);
}

void archetype_debug_impl(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    put_va("", fmt, ap);
    va_end(ap);
}

void archetype_debugN_impl(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    glk_put_string(buf);
}

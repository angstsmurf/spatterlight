/* Implementations for the Common::* / Graphics::* shims in
 * comprehend_compat.h. */

#include "comprehend_compat.h"

#include <map>

extern "C" {
#include "glk.h"
}

namespace Common {

ConfigManager ConfMan;
DebugManager DebugMan;
SearchManager SearchMan;

String String::vformat(const char *fmt, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int needed = std::vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) return String();
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

void String::toUppercase() {
    for (size_t i = 0; i < size(); ++i)
        (*this)[i] = (char)std::toupper((unsigned char)(*this)[i]);
}

bool String::hasPrefixIgnoreCase(const char *pre) const {
    size_t n = std::strlen(pre);
    if (size() < n) return false;
    for (size_t i = 0; i < n; ++i)
        if (std::tolower((unsigned char)(*this)[i]) != std::tolower((unsigned char)pre[i]))
            return false;
    return true;
}

bool String::hasSuffixIgnoreCase(const char *suf) const {
    size_t n = std::strlen(suf);
    if (size() < n) return false;
    size_t off = size() - n;
    for (size_t i = 0; i < n; ++i)
        if (std::tolower((unsigned char)(*this)[off + i]) != std::tolower((unsigned char)suf[i]))
            return false;
    return true;
}

// Greedy word wrap that breaks on spaces. ScummVM's wordWrap inserts hard
// newlines without trailing whitespace; we do the same.
void String::wordWrap(size_t maxLineWidth) {
    if (maxLineWidth == 0 || size() <= maxLineWidth) return;
    std::string out;
    size_t lineStart = 0, lastSpace = std::string::npos;
    for (size_t i = 0; i < size(); ++i) {
        char c = (*this)[i];
        if (c == '\n') {
            out.append(&(*this)[lineStart], i - lineStart + 1);
            lineStart = i + 1;
            lastSpace = std::string::npos;
            continue;
        }
        if (c == ' ') lastSpace = i;
        if (i - lineStart >= maxLineWidth && lastSpace != std::string::npos) {
            out.append(&(*this)[lineStart], lastSpace - lineStart);
            out.push_back('\n');
            lineStart = lastSpace + 1;
            lastSpace = std::string::npos;
        }
    }
    out.append(&(*this)[lineStart], size() - lineStart);
    assign(out);
}

void U32String::vformat(const uint32 *fmtBegin, const uint32 *fmtEnd,
                        U32String &out, va_list args) {
    // Reduce to ASCII printf — enough for comprehend's English UI.
    std::string fmt;
    for (const uint32 *p = fmtBegin; p != fmtEnd; ++p)
        fmt.push_back((char)*p);
    char buf[2048];
    std::vsnprintf(buf, sizeof(buf), fmt.c_str(), args);
    out.clear();
    for (const char *p = buf; *p; ++p)
        out.push_back((uint32)(unsigned char)*p);
}

// --- In-memory virtual filesystem (Apple II disk images) ---

static String fsKey(const String &path) {
    // Reduce to a lower-cased basename so "dir/G0", "g0" and "G0" all match.
    size_t slash = path.find_last_of("/\\");
    String name = (slash == std::string::npos) ? path
                                               : String(path.c_str() + slash + 1);
    name.toLowercase();
    return name;
}

namespace DiskImageFS {

typedef std::map<std::string, std::vector<byte>> Disk;

static std::vector<Disk> &disks() {
    static std::vector<Disk> d;
    return d;
}
static int s_loadDisk = -1;   // disk currently being filled by add()
static int s_activeDisk = 0;  // disk consulted first by get()/has()

void beginDisk() {
    disks().push_back(Disk());
    s_loadDisk = (int)disks().size() - 1;
}

void add(const String &name, const byte *data, size_t len) {
    if (s_loadDisk < 0)
        beginDisk();
    disks()[s_loadDisk][fsKey(name)].assign(data, data + len);
}

bool selectDiskByGameMagic(uint16 magic) {
    for (size_t i = 0; i < disks().size(); ++i) {
        auto it = disks()[i].find("g0");
        if (it != disks()[i].end() && it->second.size() >= 2 &&
                (uint16)(it->second[0] | (it->second[1] << 8)) == magic) {
            s_activeDisk = (int)i;
            return true;
        }
    }
    return false;
}

const std::vector<byte> *get(const String &name) {
    std::string key = fsKey(name);

    // Active disk wins, so multi-disk games get the right dataset.
    if (s_activeDisk >= 0 && s_activeDisk < (int)disks().size()) {
        auto it = disks()[s_activeDisk].find(key);
        if (it != disks()[s_activeDisk].end())
            return &it->second;
    }
    // Otherwise fall back to any disk (single datasets span several sides).
    for (Disk &d : disks()) {
        auto it = d.find(key);
        if (it != d.end())
            return &it->second;
    }
    return nullptr;
}

bool has(const String &name) { return get(name) != nullptr; }

bool active() { return !disks().empty(); }

void clear() {
    disks().clear();
    s_loadDisk = -1;
    s_activeDisk = 0;
}

} // namespace DiskImageFS

bool FileStream::open(const String &path, const char *mode) {
    close();
    _eos = false;

    // Serve from the disk-image VFS when one is loaded (read modes only).
    const std::vector<byte> *mem = DiskImageFS::get(path);
    if (mem && (mode == nullptr || mode[0] == 'r')) {
        _memBuf = *mem;
        _memPos = 0;
        _inMem = true;
        return true;
    }

    _f = std::fopen(path.c_str(), mode);
    return _f != nullptr;
}

void FileStream::close() {
    if (_f) { std::fclose(_f); _f = nullptr; }
    _inMem = false;
    _memBuf.clear();
    _memPos = 0;
    _eos = false;
}

byte FileStream::readByte() {
    if (_inMem) {
        if (_memPos >= _memBuf.size()) { _eos = true; return 0; }
        return _memBuf[_memPos++];
    }
    int c = _f ? std::fgetc(_f) : EOF;
    if (c == EOF) { _eos = true; return 0; }
    return (byte)c;
}

uint32 FileStream::read(void *buf, uint32 size) {
    if (_inMem) {
        size_t avail = _memBuf.size() - _memPos;
        uint32 n = (uint32)(size < avail ? size : avail);
        std::memcpy(buf, _memBuf.data() + _memPos, n);
        _memPos += n;
        if (n < size) _eos = true;
        return n;
    }
    if (!_f) return 0;
    size_t n = std::fread(buf, 1, size, _f);
    if (n < size) _eos = true;
    return (uint32)n;
}

int64 FileStream::pos() const {
    if (_inMem) return (int64)_memPos;
    return _f ? (int64)std::ftell(_f) : -1;
}

int64 FileStream::size() const {
    if (_inMem) return (int64)_memBuf.size();
    if (!_f) return -1;
    long cur = std::ftell(_f);
    std::fseek(_f, 0, SEEK_END);
    long end = std::ftell(_f);
    std::fseek(_f, cur, SEEK_SET);
    return (int64)end;
}

bool FileStream::seek(int64 offset, int whence) {
    _eos = false;
    if (_inMem) {
        int64 base = whence == SEEK_CUR ? (int64)_memPos
                   : whence == SEEK_END ? (int64)_memBuf.size() : 0;
        int64 np = base + offset;
        if (np < 0) np = 0;
        if (np > (int64)_memBuf.size()) np = (int64)_memBuf.size();
        _memPos = (size_t)np;
        return true;
    }
    if (!_f) return false;
    return std::fseek(_f, (long)offset, whence) == 0;
}

void FileStream::writeByte(byte b) { if (_f) std::fputc(b, _f); }
uint32 FileStream::write(const void *buf, uint32 size) {
    if (!_f) return 0;
    return (uint32)std::fwrite(buf, 1, size, _f);
}

bool FileStream::exists(const String &path) {
    if (DiskImageFS::has(path)) return true;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// Real RFC 1321 MD5 over the first `length` bytes of the stream. The game code
// (game_oo.cpp, game_tm.cpp, charset.cpp) fingerprints novel.exe against actual
// MD5 hashes inherited from ScummVM, so a stable-but-arbitrary hash is not
// enough -- it must be the genuine MD5 or none of those constants ever match
// (which left OO-Topos with an empty second string table, e.g. "GET ALL"
// printing BAD_STRING).
String computeStreamMD5AsString(ReadStream *stream, uint32 length) {
    static const uint32 S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };
    static const uint32 K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    // Read the bytes to hash into a buffer (MD5 needs the length up front).
    std::vector<byte> data;
    data.reserve(length);
    for (uint32 i = 0; i < length && !stream->eos(); ++i)
        data.push_back(stream->readByte());

    uint32 a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    // Pad: append 0x80, then zeros, then the 64-bit bit length (little-endian).
    uint64 bitLen = (uint64)data.size() * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56)
        data.push_back(0);
    for (int i = 0; i < 8; ++i)
        data.push_back((byte)(bitLen >> (8 * i)));

    for (size_t off = 0; off < data.size(); off += 64) {
        uint32 M[16];
        for (int i = 0; i < 16; ++i)
            M[i] = (uint32)data[off + i * 4] |
                   ((uint32)data[off + i * 4 + 1] << 8) |
                   ((uint32)data[off + i * 4 + 2] << 16) |
                   ((uint32)data[off + i * 4 + 3] << 24);

        uint32 A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; ++i) {
            uint32 F;
            int g;
            if (i < 16) {
                F = (B & C) | (~B & D);
                g = i;
            } else if (i < 32) {
                F = (D & B) | (~D & C);
                g = (5 * i + 1) & 15;
            } else if (i < 48) {
                F = B ^ C ^ D;
                g = (3 * i + 5) & 15;
            } else {
                F = C ^ (B | ~D);
                g = (7 * i) & 15;
            }
            F += A + K[i] + M[g];
            A = D;
            D = C;
            C = B;
            B += (F << S[i]) | (F >> (32 - S[i]));
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    const uint32 words[4] = { a0, b0, c0, d0 };
    char buf[33];
    for (int w = 0; w < 4; ++w)
        for (int i = 0; i < 4; ++i)
            std::snprintf(buf + w * 8 + i * 2, 3, "%02x",
                          (words[w] >> (8 * i)) & 0xff);
    buf[32] = 0;
    return String(buf);
}

String computeStreamMD5AsString(ReadStream &stream, uint32 length) {
    return computeStreamMD5AsString(&stream, length);
}

String Path::baseName() const {
    size_t p = _s.find_last_of('/');
    if (p == std::string::npos) p = _s.find_last_of('\\');
    if (p == std::string::npos) return _s;
    return String(_s.c_str() + p + 1);
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

static void put_va(const char *prefix, const char *fmt, va_list ap) {
    char buf[1024];
    buf[0] = 0;
    if (prefix && *prefix) std::snprintf(buf, sizeof(buf), "%s", prefix);
    size_t off = std::strlen(buf);
    std::vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
    size_t len = std::strlen(buf);
    if (len > 0 && buf[len - 1] != '\n' && len + 1 < sizeof(buf)) {
        buf[len] = '\n';
        buf[len + 1] = '\0';
    }
    glk_put_string(buf);
}

void comprehend_error_impl(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    put_va("Error: ", fmt, ap);
    va_end(ap);
}

void comprehend_debug_impl(const char *fmt, ...) {
    (void)fmt;  // Stub: only emit when SPATTERLIGHT_DEBUG is defined.
#ifdef SPATTERLIGHT_DEBUG
    va_list ap; va_start(ap, fmt);
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "%s\n", buf);
#endif
}

void comprehend_debugN_impl(const char *fmt, ...) {
    (void)fmt;
#ifdef SPATTERLIGHT_DEBUG
    va_list ap; va_start(ap, fmt);
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "%s", buf);
#endif
}

void comprehend_debugC_impl(int chan, const char *fmt, ...) {
    (void)chan; (void)fmt;
#ifdef SPATTERLIGHT_DEBUG
    va_list ap; va_start(ap, fmt);
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "%s\n", buf);
#endif
}

void comprehend_warning_impl(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "Warning: %s\n", buf);
}

// --- Graphics:: ---

namespace Graphics {

// Bresenham line draw matching ScummVM's ManagedSurface::drawLine.
void ManagedSurface::drawLine(int x1, int y1, int x2, int y2, uint32 color) {
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        if (x1 >= 0 && y1 >= 0 && x1 < w && y1 < h)
            ((uint32 *)getPixels())[y1 * w + x1] = color;
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

} // namespace Graphics

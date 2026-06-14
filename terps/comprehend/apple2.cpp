/* apple2.cpp -- Apple II disk-image import for the Comprehend port. */

#include "apple2.h"
#include "graphics_magician.h"
#include "graphics_magician_dhgr.h"
#include "comprehend_compat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <strings.h>

extern "C" {
// Relative paths so the build needs no extra header search paths (the
// comprehend target links libreadwoz.dylib for these symbols).
#include "../readwoz/ciderpress.h"
#include "../readwoz/woz2nib.h"
}

namespace Glk {
namespace Comprehend {

#define DSK_IMAGE_SIZE (35 * 16 * 256)   // 143360
#define NIB_IMAGE_SIZE (35 * 6656)       // 232960

static bool hasExtIgnoreCase(const Common::String &path, const char *ext) {
    size_t n = std::strlen(ext);
    if (path.size() < n) return false;
    return strcasecmp(path.c_str() + path.size() - n, ext) == 0;
}

static uint8_t *readWholeFile(const Common::String &path, size_t *outSize) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return nullptr; }
    uint8_t *data = (uint8_t *)std::malloc(sz);
    if (std::fread(data, 1, sz, f) != (size_t)sz) {
        std::free(data);
        std::fclose(f);
        return nullptr;
    }
    std::fclose(f);
    *outSize = (size_t)sz;
    return data;
}

bool isAppleDiskImage(const Common::String &path) {
    if (hasExtIgnoreCase(path, ".woz") || hasExtIgnoreCase(path, ".dsk") ||
        hasExtIgnoreCase(path, ".do")  || hasExtIgnoreCase(path, ".po")  ||
        hasExtIgnoreCase(path, ".nib"))
        return true;

    // Sniff the contents for a WOZ header or a bare 140K sector image.
    size_t sz = 0;
    uint8_t *data = readWholeFile(path, &sz);
    if (!data) return false;
    bool isImage = (sz >= 4 && data[0] == 'W' && data[1] == 'O' && data[2] == 'Z') ||
                   sz == DSK_IMAGE_SIZE || sz == NIB_IMAGE_SIZE;
    std::free(data);
    return isImage;
}

// Convert any supported image into a 143360-byte DOS-ordered .dsk buffer.
// Returns a newly malloc'd buffer (caller frees) and sets *outSize, or null.
static uint8_t *toDskImage(uint8_t *data, size_t size, size_t *outSize) {
    if (size >= 4 && data[0] == 'W' && data[1] == 'O' && data[2] == 'Z') {
        size_t nibSize = size;
        uint8_t *nib = woz2nib(data, &nibSize);
        if (!nib) return nullptr;
        uint8_t *dsk = ReadFullImageFromNib(nib, nibSize);
        std::free(nib);
        if (!dsk) return nullptr;
        *outSize = DSK_IMAGE_SIZE;
        return dsk;
    }
    if (size == NIB_IMAGE_SIZE) {
        uint8_t *dsk = ReadFullImageFromNib(data, size);
        if (!dsk) return nullptr;
        *outSize = DSK_IMAGE_SIZE;
        return dsk;
    }
    if (size >= DSK_IMAGE_SIZE) {
        uint8_t *dsk = (uint8_t *)std::malloc(DSK_IMAGE_SIZE);
        std::memcpy(dsk, data, DSK_IMAGE_SIZE);
        *outSize = DSK_IMAGE_SIZE;
        return dsk;
    }
    return nullptr;
}

// Extract every ProDOS file from one image and register the ones we don't
// already have. Returns the number of newly registered files.
static int loadOneImage(const Common::String &path) {
    size_t size = 0;
    uint8_t *raw = readWholeFile(path, &size);
    if (!raw) return 0;

    size_t dskSize = 0;
    uint8_t *dsk = toDskImage(raw, size, &dskSize);
    std::free(raw);
    if (!dsk) return 0;

    size_t numFiles = 0;
    A2FileRec *files = GetAllProDOSFiles(dsk, dskSize, &numFiles);

    // Each disk side keeps its own file set; multi-disk games (Crimson Crown,
    // Talisman) carry a different dataset per disk under the same names.
    int added = 0;
    if (numFiles > 0) {
        Common::DiskImageFS::beginDisk();
        for (size_t i = 0; i < numFiles; i++) {
            Common::DiskImageFS::add(files[i].filename, files[i].data, files[i].datasize);
            added++;
        }
    }
    for (size_t i = 0; i < numFiles; i++) {
        std::free(files[i].filename);
        std::free(files[i].data);
    }
    std::free(files);
    FreeDiskImage();
    std::free(dsk);
    return added;
}

int loadAppleDiskImage(const Common::String &path) {
    int total = loadOneImage(path);

    // Pull in companion disk sides sitting in the same directory.
    Common::String dir = ".";
    Common::String base = path;
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        dir = Common::String(path.c_str(), slash);
        base = Common::String(path.c_str() + slash + 1);
    }

    DIR *d = opendir(dir.c_str());
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr) {
            Common::String name(ent->d_name);
            if (name == base || name == "." || name == "..")
                continue;
            Common::String full = dir;
            full += '/';
            full += name;
            if (isAppleDiskImage(full))
                total += loadOneImage(full);
        }
        closedir(d);
    }

    // The Talisman boot disk's "T2" file (Graphics Magician) carries the
    // drawing tables (pattern data, fill-colour subindices, brush bitmaps)
    // used by graphics_magician.cpp. Install them now that every side has been
    // loaded; the renderer is otherwise a no-op.
    const std::vector<byte> *t2 = Common::DiskImageFS::get("t2");
    if (t2)
        gmInstallDrawingTables(t2->data(), t2->size());

    // The "T5" file is the boot disk's double-hi-res ("<D>" mode) interpreter;
    // its drawing tables sit at fixed offsets inside the headerless $4000 image.
    // Install them so the DHGR renderer (graphics_magician_dhgr.cpp) can run when
    // the player toggles double hi-res. Absent on games that ship no <D> mode,
    // in which case the toggle stays unavailable.
    const std::vector<byte> *t5 = Common::DiskImageFS::get("t5");
    if (t5)
        gmDhgrInstallDrawingTables(t5->data(), t5->size());

    return total;
}

// Map a main-data-file magic word to a Comprehend game id. Returns an empty
// string for magics we don't recognise (callers then fall back to other cues).
static Common::String gameIdForMagic(uint16 magic) {
    switch (magic) {
    case 0xa429:            // Talisman "Empire" disk (matches the DOS magic)
    case 0x94c6:            // Talisman "Lands Beyond" disk
        return Common::String("talisman");
    case 0x93f0:            // OO-Topos
        return Common::String("ootopos");
    case 0x8cba:            // Transylvania (Apple II) — the version 1 game
        return Common::String("transylvania");
    case 0x8bc3:            // Transylvania v2 (DOS)
        return Common::String("transylvaniav2");
    case 0x9f8b:            // The Coveted Mirror (Apple II)
        return Common::String("covetedmirror");
    case 0x2000:            // Crimson Crown / Transylvania v1 disk one
    case 0x84fe:            // Crimson Crown (Apple) disk one
    case 0x7dec:            // Crimson Crown (Apple) disk two
        return Common::String("crimsoncrown");
    default:
        return Common::String();
    }
}

// Read the little-endian magic word from the extracted main game-data file.
static uint16 mainDataMagic() {
    const std::vector<byte> *g0 = Common::DiskImageFS::get("g0");
    if (!g0 || g0->size() < 2) return 0;
    return (uint16)((*g0)[0] | ((*g0)[1] << 8));
}

Common::String guessAppleGameId() {
    Common::String id = gameIdForMagic(mainDataMagic());
    if (!id.empty())
        return id;

    // Fall back to filename heuristics.
    if (Common::DiskImageFS::has("ps") && Common::DiskImageFS::has("mn"))
        return Common::String("talisman");
    if (Common::DiskImageFS::has("g0"))
        return Common::String("transylvaniav2");

    return Common::String();
}

Common::String guessDosGameId() {
    // DOS/PC releases keep their files on the real filesystem (we have already
    // chdir'd into the game directory), so the engine never builds a DiskImageFS.
    // Read the magic word straight out of the "g0" main-data file -- this is what
    // distinguishes, e.g., the v2 Transylvania (g0, 0x8bc3) from the v1 release
    // (which ships tr.gda and has no g0, so the read fails and we return empty).
    uint16 magic = 0;
    const char *names[] = { "g0", "G0", nullptr };
    for (int i = 0; names[i]; i++) {
        if (FILE *f = std::fopen(names[i], "rb")) {
            uint8_t hdr[2];
            if (std::fread(hdr, 1, 2, f) == 2)
                magic = (uint16)(hdr[0] | (hdr[1] << 8));
            std::fclose(f);
            if (magic)
                break;
        }
    }
    return gameIdForMagic(magic);
}

} // namespace Comprehend
} // namespace Glk

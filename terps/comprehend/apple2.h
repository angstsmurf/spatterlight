// Apple II disk-image import. The Apple II releases store their data files (g0, RA, OA,
// MA, T0...) inside a ProDOS volume on a disk image, with the game-file directory unlinked
// from the volume root. This module extracts every file (plus companion disk sides found
// alongside it) into the in-memory DiskImageFS, so the engine opens them by name as on DOS.

#ifndef COMPREHEND_APPLE2_H
#define COMPREHEND_APPLE2_H

#include "comprehend_compat.h"

namespace Glk {
namespace Comprehend {

// True if the file at `path` looks like an Apple II disk image (.dsk/.do/.po/
// .nib/.woz, a WOZ header, or a 140K sector image).
bool isAppleDiskImage(const Common::String &path);

// Load `path` and any sibling disk images in the same directory into the
// DiskImageFS. Earlier-loaded files win on name collisions, so the opened
// disk's own files take precedence over companion sides. Returns the number
// of files registered (0 if the image could not be read).
int loadAppleDiskImage(const Common::String &path);

// After loadAppleDiskImage(), guess the Comprehend game id ("talisman",
// "ootopos", ...) from the extracted files. Empty string if undetermined.
Common::String guessAppleGameId();

// Guess the game id for a DOS/PC release by reading the "g0" main-data file's
// magic word from the current directory. Empty string if undetermined (e.g. the
// v1 games, which ship tr.gda/cc1.gda rather than g0).
Common::String guessDosGameId();

} // namespace Comprehend
} // namespace Glk

#endif

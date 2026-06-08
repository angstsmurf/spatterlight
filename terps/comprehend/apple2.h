/* apple2.h -- Apple II disk-image import for the Comprehend port.
 *
 * The Apple II releases of the Penguin/Polarware "Comprehend" games store
 * their data files (g0, RA, OA, MA, T0...) inside a ProDOS volume on a
 * DOS-sector-ordered .dsk / .woz / .nib disk image, with the game-file
 * directory deliberately unlinked from the volume root. This module detects
 * such an image, extracts every file (plus any companion disk sides found
 * alongside it) and registers them in the in-memory DiskImageFS so the engine
 * can open them by name exactly as it would the DOS/PC files.
 *
 * Mirrors how terps/scott/saga/apple2detect.c feeds the ScottFree engine.
 */

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

} // namespace Comprehend
} // namespace Glk

#endif

/* autosavefiles.h

   The autosave file pair that a terp keeps in its autosave directory
   (getautosavedir in fileref.m):

     autosave.glksave   the engine's own state, as opaque bytes
     autosave.plist     the Glk library state (TempLibrary), with whatever
                        the terp's archive hook appends

   Writing goes to autosave-tmp.* first and only then renames both into
   place, keeping the previous pair as autosave-bak.*, so the files on disk
   never mix two turns.  The terp keeps only what is its own: building the
   engine state, and the archive/unarchive hooks that carry its frontend
   globals (window tags and the like) inside the plist.

   gamepath is the game file getautosavedir hashes to find the directory.
*/

#ifndef GLKIMP_AUTOSAVEFILES
#define GLKIMP_AUTOSAVEFILES

#include <stdbool.h>
#include <stddef.h>

#ifdef __OBJC__
@class TempLibrary;
@class NSCoder;
typedef void (*gli_autosave_hook)(TempLibrary *, NSCoder *);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* True if autosaving is enabled and a complete pair exists.  A glksave
 * with no plist next to it can never be restored, so it is deleted. */
bool gli_autosave_exists(const char *gamepath);

/* The per-prompt guard: autosaving enabled, and the last Glk event was one
 * worth saving after (not the first prompt, not an arrange or redraw
 * wakeup, and a timer event only with the autosave-on-timer preference). */
bool gli_autosave_wanted(void);

/* Delete the pair and its -bak copies, e.g. after a failed restore, so the
 * next launch starts fresh instead of failing the same way. */
void gli_autosave_discard(const char *gamepath);

/* Read autosave.glksave into a malloc'ed buffer the caller frees. */
bool gli_autosave_read_game(const char *gamepath, void **data, size_t *length);

#ifdef __OBJC__

/* Write the engine state and the Glk library state (archive_hook, if any,
 * appends the terp's extras) as a pair, then ask the window server to
 * snapshot the GUI under the same tag.  Returns false, with the previous
 * pair left in place, if anything fails. */
bool gli_autosave_write(const char *gamepath, const void *data, size_t length,
                        gli_autosave_hook archive_hook);

/* Unarchive autosave.plist (unarchive_hook, if any, reads the terp's
 * extras back), or return nil.  Nothing live is replaced: the caller runs
 * updateFromLibrary, re-points its own globals, then updateFromLibraryLate. */
TempLibrary *gli_autosave_load_library(const char *gamepath,
                                       gli_autosave_hook unarchive_hook);

#endif /* __OBJC__ */

#ifdef __cplusplus
}
#endif

#endif /* GLKIMP_AUTOSAVEFILES */

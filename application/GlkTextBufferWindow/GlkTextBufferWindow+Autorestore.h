#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Autosave serialization and deserialization (NSSecureCoding), post-restore
 * adjustments, and restoration of the scroll bar style and find bar state.
 */

// restoreScrollBarStyle and restoreTextFinder, which are called from outside
// the class, are declared in GlkTextBufferWindow.h.
@interface GlkTextBufferWindow (Autorestore)
@end

NS_ASSUME_NONNULL_END

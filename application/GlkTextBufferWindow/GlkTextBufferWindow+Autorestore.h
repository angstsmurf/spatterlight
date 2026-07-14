#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Autosave serialization and deserialization (NSSecureCoding), post-restore
 * adjustments, and restoration of the scroll bar style and find bar state.
 */

@interface GlkTextBufferWindow (Autorestore)

- (void)restoreScrollBarStyle;
- (void)restoreTextFinder;

@end

NS_ASSUME_NONNULL_END

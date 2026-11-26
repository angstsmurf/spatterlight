#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Colors and Glk styles: background recalculation, theme/preference changes,
 * and the Beyond Zork font 3 style.
 */

@interface GlkTextBufferWindow (Styles)

- (void)recalcBackground;
- (void)createBeyondZorkStyle;

// Also declared in GlkTextBufferWindow.h; implemented in this category.
- (void)restoreScrollBarStyle;

@end

NS_ASSUME_NONNULL_END

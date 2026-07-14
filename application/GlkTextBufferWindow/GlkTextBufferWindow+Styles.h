#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Colors and Glk styles: background recalculation, theme/preference changes,
 * and the Beyond Zork Font3 pseudo-graphics style.
 */

@interface GlkTextBufferWindow (Styles)

- (void)recalcBackground;
- (void)createBeyondZorkStyle;
- (NSDictionary *)font3ToUnicode;

@end

NS_ASSUME_NONNULL_END

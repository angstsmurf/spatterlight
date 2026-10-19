#import "GlkTextGridWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Theme styles, the background colour taken from the grid, and the Beyond
 * Zork graphical font.
 */

@interface GlkTextGridWindow (Styles)

- (void)checkForUglyBorder;
- (void)fixCollapsingSpaceRows;
- (void)createBeyondZorkStyle;

@end

NS_ASSUME_NONNULL_END

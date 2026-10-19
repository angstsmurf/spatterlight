#import "GlkTextGridWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Hyperlink, mouse, character and line input, and the command history.
 */

@interface GlkTextGridWindow (Input)

// Re-open line input with text already typed, after delay seconds. A newer
// call cancels one still waiting.
- (void)scheduleDeferredInitLine:(NSString *)text delay:(NSTimeInterval)delay;

@end

NS_ASSUME_NONNULL_END

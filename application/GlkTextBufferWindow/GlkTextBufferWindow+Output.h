#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Game text output: printing styled text, clearing the window, and
 * retracting printed text.
 */

@interface GlkTextBufferWindow (Output)

- (void)printToWindow:(NSString *)str style:(NSUInteger)stylevalue;
- (void)reallyClear;
- (void)clearScrollback:(nullable id)sender;
- (void)echo:(BOOL)val;

@end

NS_ASSUME_NONNULL_END

#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Character and line input: key event dispatch, line submission, command
 * history, the fence-enforcing NSTextView delegate methods, and the
 * insertion point.
 */

@interface GlkTextBufferWindow (Input)

- (void)sendInputLine:(NSString *)line withTerminator:(NSUInteger)terminator;
- (void)recalcInputAttributes;
- (void)showInsertionPoint;
- (void)hideInsertionPoint;

@end

NS_ASSUME_NONNULL_END

#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Inline and margin images: drawing, scaling, flow breaks, and attachment
 * rescaling after resizes and theme changes.
 */

@interface GlkTextBufferWindow (Images)

- (void)updateImageAttachmentsWithXScale:(CGFloat)xscale yScale:(CGFloat)yscale;
// Start playing any animated images in the buffer. Does nothing if they are
// already playing; stops by itself when the buffer holds none.
- (void)startImageAnimations;

@end

NS_ASSUME_NONNULL_END

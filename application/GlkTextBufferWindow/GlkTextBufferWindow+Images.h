#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Inline and margin images: drawing, scaling, flow breaks, and attachment
 * rescaling after resizes and theme changes.
 */

@interface GlkTextBufferWindow (Images)

- (void)updateImageAttachmentsWithXScale:(CGFloat)xscale yScale:(CGFloat)yscale;

@end

NS_ASSUME_NONNULL_END

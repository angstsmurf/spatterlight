//
//  BufferTextView.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Cocoa/Cocoa.h>
#import "FindableTextView.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Extend NSTextView to ...
 *   - redirect keyDown to our GlkTextBufferWindow object
 *   - draw images with high quality interpolation
 *   - extend bottom to show images that extend below bottom of text
 *   - hide text input cursor when desirable
 *   - use custom search bar
 *   - customize contextual menu
 */

@interface BufferTextView : FindableTextView <NSSecureCoding>

- (void)superKeyDown:(NSEvent *)evt;
- (void)temporarilyHideCaret;
- (void)enableCaret:(nullable id)sender;

@property BOOL shouldDrawCaret;
// Extra height below the text that keeps a low margin image inside the
// frame. Worked out from the margin images in setFrameSize:, so only the
// archive and the view-swap code should set it directly.
@property CGFloat bottomPadding;

// Re-work out bottomPadding from the current margin image bounds and resize
// to match. Call once layout has settled, before scrolling.
- (void)updateBottomPadding;
// The same, deferred to the next run loop pass, and only if the padding is
// out of date. Safe to call while drawing.
- (void)bottomPaddingNeedsUpdate;

@end

NS_ASSUME_NONNULL_END

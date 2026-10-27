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

// VoiceOver (issue #80): the move element reported as the focused UI
// element while VoiceOver is parked on the latest move. Cleared as soon as
// the player types. See GlkTextBufferWindow armVoiceOverFocusedMoveElement.
@property (nullable, weak) id voFocusedMoveElement;

// "Limit VoiceOver to Last Move" (Speech menu, kVOTailModeKey): VoiceOver
// reads a text area from its first character whenever its cursor lands on
// it, and scrolls there. With the setting on, the accessible text starts
// where the last move starts, so landing on the text view reads the last
// move and the scrollback stays put. Every character index VoiceOver sees
// is offset by accessibilityTextOffset; the full text stays available to
// our own code through the ...ForFullTextRange: method.
@property (readonly) NSUInteger accessibilityTextOffset;
- (NSAttributedString *)accessibilityAttributedStringForFullTextRange:(NSRange)range;

@end

NS_ASSUME_NONNULL_END

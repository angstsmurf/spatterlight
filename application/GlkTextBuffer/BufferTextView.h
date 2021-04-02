//
//  BufferTextView.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Cocoa/Cocoa.h>

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

@interface BufferTextView : NSTextView <NSTextFinderClient, NSSecureCoding>

- (void)superKeyDown:(NSEvent *)evt;
- (void)temporarilyHideCaret;
- (void)resetTextFinder; // Call after changing the text storage, or search will
                         // break.
- (void)enableCaret:(nullable id)sender;

@property BOOL shouldDrawCaret;
@property CGFloat bottomPadding;
@property(weak, readonly) NSTextFinder *textFinder;

@end

NS_ASSUME_NONNULL_END

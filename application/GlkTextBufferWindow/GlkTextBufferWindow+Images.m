#import "GlkTextBufferWindowPrivate.h"

#import "GlkController.h"
#import "Game.h"
#import "ImageHandler.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "MyAttachmentCell.h"
#import "BufferTextView.h"
#include "glkimp.h"
#import <QuartzCore/QuartzCore.h>

// In release builds, suppress NSLog entirely.
#ifndef DEBUG
#define NSLog(...)
#endif // DEBUG

static const NSUInteger kMaxMarginImages = 200;

@implementation GlkTextBufferWindow (Images)

// Scale an image to the specified size using high-quality interpolation.
// Returns the original if it already matches the target dimensions.
- (NSImage *)scaleImage:(NSImage *)src size:(NSSize)dstsize {
    NSSize srcsize = src.size;
    NSImage *dst;

    if (NSEqualSizes(srcsize, dstsize))
        return src;

    dst = [[NSImage alloc] initWithSize:dstsize];
    [dst lockFocus];

    [NSGraphicsContext currentContext].imageInterpolation =
    NSImageInterpolationHigh;

    [src drawInRect:NSMakeRect(0, 0, dstsize.width, dstsize.height)
           fromRect:NSMakeRect(0, 0, srcsize.width, srcsize.height)
          operation:NSCompositingOperationSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:nil];

    [dst unlockFocus];

    return dst;
}

// Create an NSTextAttachment wrapping a MyAttachmentCell for an inline image.
// The cell stores the Glk alignment mode and image resource index for later
// identification during rescaling and accessibility queries.
- (NSTextAttachment *)textAttachmenWithImage:(NSImage *)image alignment:(NSInteger)alignment index:(NSInteger)index position:(NSUInteger)position {
    NSTextAttachment *att = [[NSTextAttachment alloc] initWithData:nil ofType:nil];
    MyAttachmentCell *cell =
    [[MyAttachmentCell alloc] initImageCell:image
                               andAlignment:alignment
                                  andAttStr:textstorage
                                         at:position
                                      index:index];
    att.attachmentCell = cell;
    return att;
}

// Insert an image into the text buffer. Handles both inline images
// (InlineUp/Down/Center) and margin-floated images (MarginLeft/Right).
// Margin images are added to the MarginContainer's image list and require
// the preceding character to be a newline. Inline images are appended as
// text attachments. If width or height is 0, uses the image's natural size.
// A nonzero imagerule (glk_image_draw_scaled_ext, Glk 0.7.6) carries the
// rule's raw width/height/maxwidth arguments instead: inline cells store the
// rule and resolve their display size at layout time (tracking resizes);
// margin images resolve it once here.
- (void)drawImage:(NSImage *)image
             val1:(NSInteger)alignment
             val2:(NSInteger)index
            width:(NSInteger)w
           height:(NSInteger)h
        imagerule:(NSUInteger)imagerule
         maxwidth:(NSUInteger)maxwidth
            style:(NSUInteger)style {
    [self flushDisplay];

    if (storedNewline) {
        [textstorage appendAttributedString:storedNewline];
        storedNewline = nil;
        _lastchar = '\n';
    }

    BOOL isMargin = (alignment == imagealign_MarginLeft ||
                     alignment == imagealign_MarginRight);
    NSUInteger cellRule = 0;
    NSSize natural = image.size;

    if (imagerule && isMargin) {
        // Margin images are laid out by the MarginContainer, not the cell:
        // resolve the rule once against the current content width. (The
        // spec's dynamic behaviour is buffer-inline-specific; maxwidth
        // still bounds the initial size.)
        NSSize resolved = [MyAttachmentCell resolveImageRule:imagerule
                                                       width:(NSUInteger)w
                                                      height:(NSUInteger)h
                                                    maxwidth:maxwidth
                                                     natural:natural
                                                   available:scrollview.contentView.frame.size.width];
        w = (NSInteger)resolved.width;
        h = (NSInteger)resolved.height;
        imagerule = 0;
    }

    if (imagerule) {
        // Rule-scaled inline image: keep the image at its natural size and
        // let the attachment cell resolve the display size per layout pass.
        cellRule = imagerule;
    } else {
        if (w == 0)
            w = (NSInteger)image.size.width;
        if (h == 0)
            h = (NSInteger)image.size.height;

        image = [self scaleImage:image size:NSMakeSize((CGFloat)w, (CGFloat)h)];
    }

    if (textstorage.length == 0 && isMargin) {
        [textstorage appendAttributedString:[[NSAttributedString alloc] initWithString:@"\u00AD" attributes:styles[style]]];
        _lastchar = '\n';
    }

    MyAttachmentCell *cell =
    [[MyAttachmentCell alloc] initImageCell:image
                               andAlignment:alignment
                                  andAttStr:textstorage
                                         at:textstorage.length
                                      index:index];

    // The image being drawn is always the one the handler loaded last.
    cell.imageNumber = self.glkctl.imageHandler.lastimageresno;
    if (!isMargin && cell.imageNumber >= 0) {
        cell.animation = [self.glkctl.imageHandler animationForImageNumber:cell.imageNumber];
        cell.animationResolved = YES;
        if (cell.animation)
            [self startImageAnimations];
    }

    if (cellRule) {
        cell.imagerule = cellRule;
        cell.ruleWidth = (NSUInteger)w;
        cell.ruleHeight = (NSUInteger)h;
        cell.ruleMaxWidth = maxwidth;
        cell.naturalSize = natural;
    }

    if (alignment == imagealign_MarginLeft || alignment == imagealign_MarginRight) {
        if (_lastchar != '\n') {
            NSLog(@"lastchar is not line break. Do not add margin image.");
            return;
        }

        // Old margin images normally go away when the scrollback is trimmed.
        // This cap only matters with unlimited scrollback. Evicting forces a
        // re-layout of the whole buffer, so do it rarely and in bulk.
        if (container.marginImages.count >= kMaxMarginImages) {
            [container.marginImages removeObjectsInRange:NSMakeRange(0, kMaxMarginImages / 2)];
            // Removing an image deletes its margin float, so the text
            // that wrapped beside it reclaims the full width and reflows
            // upward. Uncache the surviving images' bounds so they track the
            // new layout instead of lagging at their stale (lower) cached
            // positions, which otherwise makes them drift down into the text.
            [container invalidateLayout:nil];
        }

        [container addImage:image alignment:alignment at:textstorage.length linkid:(NSUInteger)self.currentHyperlink];
        cell.marginImage = container.marginImages.lastObject;
        cell.marginImgUUID = cell.marginImage.uuid;
    }

    NSTextAttachment *att = [[NSTextAttachment alloc] initWithData:nil ofType:nil];
    att.attachmentCell = cell;
    NSAttributedString *attstr = [NSAttributedString
                                  attributedStringWithAttachment:att];

    [textstorage appendAttributedString:attstr];

    if (self.currentHyperlink) {
        [textstorage addAttribute:NSLinkAttributeName value:@(self.currentHyperlink) range:NSMakeRange(textstorage.length - 1, 1)];
    }

    [textstorage addAttributes:styles[style] range:NSMakeRange(textstorage.length - 1, 1)];
}

#pragma mark Animated images

// How often to look again while there are animated images in the buffer but
// none of them is playing (scrolled out of view, window hidden, Reduce
// Motion on).
static const NSTimeInterval kImageAnimationIdleInterval = 0.5;
static const NSTimeInterval kImageAnimationMinInterval = 1.0 / 60;

- (void)startImageAnimations {
    if (!imageAnimationTimer)
        [self scheduleImageAnimationTick:kImageAnimationMinInterval];
}

- (void)scheduleImageAnimationTick:(NSTimeInterval)delay {
    __weak GlkTextBufferWindow *weakSelf = self;
    imageAnimationTimer = [NSTimer timerWithTimeInterval:delay repeats:NO block:^(NSTimer *timer) {
        [weakSelf tickImageAnimations];
    }];
    // Common modes, so that playback carries on while scrolling.
    [[NSRunLoop mainRunLoop] addTimer:imageAnimationTimer forMode:NSRunLoopCommonModes];
}

// Advance every animated inline image that is on screen to the frame it
// should be showing now, and schedule the next tick for when the earliest
// of them is due to change again. Images that are not on screen are left
// alone, so a long scrollback costs nothing to animate. Margin images are
// not animated.
- (void)tickImageAnimations {
    imageAnimationTimer = nil;

    BOOL play = self.window.visible &&
        (self.window.occlusionState & NSWindowOcclusionStateVisible) &&
        !NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion;

    NSRange visibleRange = NSMakeRange(0, 0);
    NSPoint origin = _textview.textContainerOrigin;
    if (play) {
        NSRect visibleRect = NSOffsetRect(_textview.visibleRect, -origin.x, -origin.y);
        NSRange glyphRange = [layoutmanager glyphRangeForBoundingRectWithoutAdditionalLayout:visibleRect
                                                                            inTextContainer:container];
        visibleRange = [layoutmanager characterRangeForGlyphRange:glyphRange actualGlyphRange:NULL];
    }

    CFTimeInterval now = CACurrentMediaTime();
    __block BOOL anyAnimated = NO;
    __block NSTimeInterval nextTick = kImageAnimationIdleInterval;

    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:NSMakeRange(0, textstorage.length)
     options:0
     usingBlock:^(NSTextAttachment *value, NSRange subrange, BOOL *stop) {
        MyAttachmentCell *cell = (MyAttachmentCell *)value.attachmentCell;
        if (![cell isKindOfClass:[MyAttachmentCell class]] ||
            cell.glkImgAlign == imagealign_MarginLeft || cell.glkImgAlign == imagealign_MarginRight)
            return;

        // Cells restored from an autosave arrive without their animation.
        if (!cell.animationResolved) {
            if (cell.imageNumber >= 0)
                cell.animation = [self.glkctl.imageHandler animationForImageNumber:cell.imageNumber];
            cell.animationResolved = YES;
        }
        ImageAnimation *animation = cell.animation;
        if (!animation)
            return;
        anyAnimated = YES;

        if (!NSLocationInRange(subrange.location, visibleRange))
            return;

        if (cell.animationStart == 0)
            cell.animationStart = now;
        NSTimeInterval untilNext;
        NSUInteger frame = [animation frameAtTime:now - cell.animationStart untilNext:&untilNext];
        if (frame != cell.animationFrame) {
            cell.animationFrame = frame;
            NSRange glyphRange = [layoutmanager glyphRangeForCharacterRange:subrange actualCharacterRange:NULL];
            NSRect rect = [layoutmanager boundingRectForGlyphRange:glyphRange inTextContainer:container];
            [_textview setNeedsDisplayInRect:NSOffsetRect(rect, origin.x, origin.y)];
        }
        if (untilNext < nextTick)
            nextTick = untilNext;
    }];

    if (anyAnimated)
        [self scheduleImageAnimationTick:MAX(nextTick, kImageAnimationMinInterval)];
}

// Insert a flow break marker into the text storage. This tells the
// MarginContainer to stop floating margin images past this point,
// forcing subsequent text to clear below them.
- (void)flowBreak {
    [self flushDisplay];
    [_textview resetTextFinder];

    unichar uc = NSAttachmentCharacter;
    [textstorage.mutableString appendString:[NSString stringWithCharacters:&uc
                                                                    length:1]];
    // Appending via mutableString makes the new character inherit the
    // attributes of the preceding one. When a flow break immediately follows an
    // inline image, that preceding character is the image's attachment, so the
    // marker would carry NSAttachmentAttributeName and both render the image a
    // second time and show up as a phantom entry in the VoiceOver image rotor.
    // Strip any inherited attachment (and link) so the marker stays inert.
    NSRange markerRange = NSMakeRange(textstorage.length - 1, 1);
    [textstorage removeAttribute:NSAttachmentAttributeName range:markerRange];
    [textstorage removeAttribute:NSLinkAttributeName range:markerRange];
    [container flowBreakAt:textstorage.length - 1];
}

// NSTextViewDelegate: Handle drag of an image attachment. Clears the
// selection and initiates a drag session using the game's filename
// as the base name for the exported image file.
- (void)textView:(NSTextView *)view
     draggedCell:(id<NSTextAttachmentCell>)cell
          inRect:(NSRect)rect
           event:(NSEvent *)event
         atIndex:(NSUInteger)charIndex {
    _textview.selectedRange = NSMakeRange(0, 0);
    if ([cell isKindOfClass:[MyAttachmentCell class]]) {
        MyAttachmentCell *attachment = (MyAttachmentCell *)cell;
        NSString *filename = self.glkctl.game.path.lastPathComponent.stringByDeletingPathExtension;
        [attachment dragTextAttachmentFrom:view event:event filename:filename inRect:rect];
    }
}

// Rescale all image attachments (both inline and margin) by the given
// factors. Called when the window is resized or theme changes to keep
// images proportional. Skips during live resize for performance.
// Large inline images (>70% of content width) are additionally clamped
// to fit the visible area. Margin images are removed and re-added to
// the container at the new size.
- (void)updateImageAttachmentsWithXScale:(CGFloat)xscale yScale:(CGFloat)yscale {

    if (xscale == 0 || yscale == 0 || _textview.inLiveResize)
        return;

    NSMutableDictionary<NSString *, MarginImage *> *marginImages = [[NSMutableDictionary alloc] initWithCapacity:container.marginImages.count];
    for (MarginImage *marginImage in container.marginImages) {
        marginImages[marginImage.uuid] = marginImage;
    }

    // Looking images up below makes each of them the handler's "last image"
    // in turn. Put back the one the interpreter last asked for when done, or
    // a draw request that follows would draw the wrong picture.
    ImageHandler *handler = self.glkctl.imageHandler;
    NSImage *savedLastImage = handler.lastimage;
    NSInteger savedLastImageResno = handler.lastimageresno;

    [textstorage
     enumerateAttribute:NSAttachmentAttributeName
     inRange:NSMakeRange(0, textstorage.length)
     options:0
     usingBlock:^(NSTextAttachment *value, NSRange subrange, BOOL *stop) {
        if (!value) {
            return;
        }
        MyAttachmentCell *cell = (MyAttachmentCell *)value.attachmentCell;
        if (![cell isKindOfClass:[MyAttachmentCell class]])
            return;

        // Rule-scaled cells (glk_image_draw_scaled_ext) resolve their display
        // size against the wrap width at layout time; scaling their stored
        // image here would compound with that.
        if (cell.imagerule)
            return;

        NSImage *img = nil;
        BOOL imageIsMargin = (cell.glkImgAlign == imagealign_MarginLeft || cell.glkImgAlign == imagealign_MarginRight);

        // The resource number, that is, not cell.index, which is whatever
        // the game passed as val2 of glk_image_draw(). Cells from autosaves
        // older than imageNumber do not know theirs; fall back on the index
        // for those, as Bocfel (the only caller) passes the picture number.
        NSInteger resno = cell.imageNumber >= 0 ? cell.imageNumber : (NSInteger)cell.index;
        if ([handler handleFindImageNumber:resno]) {
            CGFloat blockXScale = xscale;
            CGFloat blockYScale = yscale;

            img = handler.lastimage;
            if (!imageIsMargin && cell.image && cell.image.size.width > scrollview.contentView.frame.size.width * 0.7) {
                CGFloat width = scrollview.contentView.frame.size.width;
                CGFloat factor = img.size.width * xscale / width;
                blockXScale *= factor;
                blockYScale *= factor;
            }
            img = [self scaleImage:img size:NSMakeSize(img.size.width * blockXScale, img.size.height * blockYScale)];
        } else {
            return;
        }

        // Replace non-margin inline images (alignment imagealign_InlineUp, imagealign_InlineDown, or imagealign_InlineCenter)
        if (!imageIsMargin) {
            NSTextAttachment *att = [self textAttachmenWithImage:img alignment:cell.glkImgAlign index:cell.index position:subrange.location];
            MyAttachmentCell *newCell = (MyAttachmentCell *)att.attachmentCell;
            newCell.imageNumber = cell.imageNumber;
            newCell.animation = cell.animation;
            newCell.animationResolved = cell.animationResolved;
            newCell.animationStart = cell.animationStart;
            newCell.animationFrame = cell.animationFrame;
            [textstorage addAttribute:NSAttachmentAttributeName value:att range:subrange];
            return;
        }

        MarginImage *mimg = marginImages[cell.marginImgUUID];
        if (mimg == nil) {
            NSLog(@"updateImageAttachments: Could not find margin image with uuid %@", cell.marginImgUUID);
            return;
        }

        [container.marginImages removeObject:mimg];
        [container addImage:img alignment:mimg.glkImgAlign at:subrange.location linkid:0];
        cell.marginImage = container.marginImages.lastObject;
        cell.marginImgUUID = cell.marginImage.uuid;
    }];

    handler.lastimage = savedLastImage;
    handler.lastimageresno = savedLastImageResno;
}

@end

//
//  MarginContainer.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Foundation/Foundation.h>
#import <AppKit/NSTextContainer.h>

@class MarginImage;

NS_ASSUME_NONNULL_BEGIN

/*
 * Extend NSTextContainer to have images in the margins with
 * the text flowing around them.
 */

@interface MarginContainer : NSTextContainer <NSSecureCoding>

@property NSMutableArray<MarginImage *> *marginImages;

- (instancetype)initWithContainerSize:(NSSize)size;
- (void)clearImages;
- (void)addImage:(NSImage *)image
       alignment:(NSInteger)alignment
              at:(NSUInteger)top
          linkid:(NSUInteger)linkid;
- (void)drawRect:(NSRect)rect;
- (void)invalidateLayout:(nullable id)sender;
- (void)unoverlap:(MarginImage *)image;
// After the first `cut` characters of the text buffer have been deleted
// (scrollback trim), drop images/flow breaks whose anchor was removed and
// shift the survivors' positions down by `cut`.
- (void)shiftAnchorsAfterTrimOf:(NSUInteger)cut;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL hasMarginImages;
- (NSUInteger)findHyperlinkAt:(NSPoint)p;
- (void)flowBreakAt:(NSUInteger)pos;
- (nullable MarginImage *)marginImageAt:(NSPoint)p;

@end

NS_ASSUME_NONNULL_END

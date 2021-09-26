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

- (id)initWithContainerSize:(NSSize)size;
- (void)clearImages;
- (void)addImage:(NSImage *)image
           align:(NSInteger)align
              at:(NSUInteger)top
          linkid:(NSUInteger)linkid;
- (void)drawRect:(NSRect)rect;
- (void)invalidateLayout:(nullable id)sender;
- (void)unoverlap:(MarginImage *)image;
- (BOOL)hasMarginImages;
- (NSUInteger)findHyperlinkAt:(NSPoint)p;
- (void)flowBreakAt:(NSUInteger)pos;
- (nullable MarginImage *)marginImageAt:(NSPoint)p;

@end

NS_ASSUME_NONNULL_END

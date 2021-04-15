//
//  MarginImage.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Cocoa/Cocoa.h>

@class MarginContainer;

NS_ASSUME_NONNULL_BEGIN

/*
 * Extend NSTextContainer to handle images in the margins,
 * with the text flowing around them like in HTML.
 */

@interface MarginImage : NSAccessibilityElement <NSSecureCoding>

@property(strong) NSImage *image;
@property(readonly) NSInteger alignment;
@property NSUInteger pos;
@property NSRect bounds;
@property NSUInteger linkid;
@property MarginContainer *container;

- (instancetype)initWithImage:(NSImage *)animage
                        align:(NSInteger)analign
                       linkid:(NSUInteger)linkid
                           at:(NSUInteger)apos
                       sender:(id)sender;

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout;
- (void)uncacheBounds;
- (NSString *)customA11yLabel;

@end

NS_ASSUME_NONNULL_END

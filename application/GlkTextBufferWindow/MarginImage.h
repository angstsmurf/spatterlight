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

@interface MarginImage : NSAccessibilityElement <NSSecureCoding, NSFilePromiseProviderDelegate, NSDraggingSource, NSPasteboardItemDataProvider>

@property (strong) NSImage *image;
@property (readonly) NSInteger glkImgAlign;
@property NSUInteger pos;
@property NSRect bounds;
@property NSUInteger linkid;
@property NSString *uuid;
@property (weak) MarginContainer *container;

- (instancetype)initWithImage:(NSImage *)animage
                    alignment:(NSInteger)analign
                       linkId:(NSUInteger)linkId
                           at:(NSUInteger)apos
                       sender:(id)sender;

- (NSRect)boundsWithLayout:(NSLayoutManager *)layout;
- (void)uncacheBounds;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString * _Nonnull customA11yLabel;

- (void)dragMarginImageFrom:(NSTextView *)source event:(NSEvent *)event filename:(NSString *)filename rect:(NSRect)rect;

-(void)cursorUpdate;


@end

NS_ASSUME_NONNULL_END

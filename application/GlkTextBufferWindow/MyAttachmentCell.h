//
//  MyAttachmentCell.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-02.
//

#import <Cocoa/Cocoa.h>

@class MarginImage;

NS_ASSUME_NONNULL_BEGIN

// I suppose this is necessary to get rid of that ugly Markup menu on attached
// images.

@interface MyAttachmentCell : NSTextAttachmentCell <NSSecureCoding, NSPasteboardItemDataProvider, NSDraggingSource, NSFilePromiseProviderDelegate>

@property (weak) NSAttributedString *attrstr;
@property (weak) MarginImage *marginImage;
@property NSUInteger pos;
@property NSInteger glkImgAlign;
@property NSInteger index;
@property NSString *marginImgUUID;

@property BOOL hasDescription;

// Glk 0.7.6 (GLK_MODULE_IMAGE2) scaling rule, from glk_image_draw_scaled_ext.
// 0 = legacy draw call. Nonzero: ruleWidth/ruleHeight are the rule's raw
// arguments and ruleMaxWidth the fixed-point ($10000 = 1.0) window-width
// bound. The display size is resolved against the line-fragment width in
// cellFrameForTextContainer:, so it tracks buffer-window resizes. Legacy
// cells get the 0.7.6 behaviour change (images wider than the window are
// reduced proportionally) through the same path.
@property NSUInteger imagerule;
@property NSUInteger ruleWidth;
@property NSUInteger ruleHeight;
@property NSUInteger ruleMaxWidth;
@property NSSize naturalSize;

// Resolve a 0.7.6 imagerule against an available width. Exposed for margin
// images, whose size is resolved once at draw time instead.
+ (NSSize)resolveImageRule:(NSUInteger)imagerule
                     width:(NSUInteger)ruleWidth
                    height:(NSUInteger)ruleHeight
                  maxwidth:(NSUInteger)maxwidth
                   natural:(NSSize)natural
                 available:(CGFloat)availwidth;

- (instancetype)initImageCell:(NSImage *)image
                 andAlignment:(NSInteger)analignment
                    andAttStr:(NSAttributedString *)anattrstr
                           at:(NSUInteger)apos
                        index:(NSInteger)index;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString * _Nonnull customA11yLabel;
- (void)dragTextAttachmentFrom:(NSTextView *)source event:(NSEvent *)event filename:(NSString *)filename inRect:(NSRect)frame;

@end

NS_ASSUME_NONNULL_END

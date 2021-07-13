//
//  ImageView.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <Cocoa/Cocoa.h>

@class Game;

NS_ASSUME_NONNULL_BEGIN

@interface ImageView : NSControl <NSDraggingDestination, NSDraggingSource, NSFilePromiseProviderDelegate, NSPasteboardTypeOwner, NSPasteboardItemDataProvider>

@property BOOL isSelected;
@property BOOL isReceivingDrag;
@property BOOL isPlaceholder;

@property NSInteger numberForSelfSourcedDrag;
@property Game *game;
@property (nullable, readonly) NSImage *image;
@property NSSet<NSPasteboardType> *acceptableTypes;

@property NSSize intrinsic;
@property CGFloat ratio;

- (instancetype)initWithGame:(Game *)game image:(nullable NSImage *)anImage;
- (NSData *)pngData;
- (void)processImage:(NSImage *)image;
- (void)superMouseDown:(NSEvent*)event;
- (void)paste:(id)sender;

@end

NS_ASSUME_NONNULL_END

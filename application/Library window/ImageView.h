//
//  ImageView.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <Cocoa/Cocoa.h>

@class Game;

NS_ASSUME_NONNULL_BEGIN

@interface ImageView : NSControl <NSDraggingDestination, NSDraggingSource, NSFilePromiseProviderDelegate, NSPasteboardTypeOwner>

@property BOOL isSelected;
@property BOOL isReceivingDrag;
@property BOOL isPlaceholder;

@property NSInteger numberForSelfSourcedDrag;
@property Game *game;
@property (nullable, readonly) NSImage *image;
@property NSSet<NSPasteboardType> *acceptableTypes;

@property NSSize intrinsic;

- (instancetype)initWithGame:(Game *)game image:(nullable NSImage *)anImage;
- (NSData *)pngData;
- (void)processImage:(NSImage *)image;

@end

NS_ASSUME_NONNULL_END

//
//  ImageView.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <Cocoa/Cocoa.h>

@class Game;

NS_ASSUME_NONNULL_BEGIN

@interface ImageView : NSControl <NSDraggingDestination, NSDraggingSource, NSPasteboardItemDataProvider, NSFilePromiseProviderDelegate, NSPasteboardTypeOwner>

@property BOOL isSelected;
@property BOOL isReceivingDrag;
@property NSInteger numberForSelfSourcedDrag;
@property Game *game;
@property (readonly) NSImage *image;
@property NSSet<NSPasteboardType> *acceptableTypes;


- (instancetype)initWithGame:(Game *)game image:(nullable NSImage *)anImage;
- (NSData *)pngData;

@end

NS_ASSUME_NONNULL_END

//
//  ImageView.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <Cocoa/Cocoa.h>

@class Game;

NS_ASSUME_NONNULL_BEGIN

@interface ImageView : NSControl <NSDraggingDestination>

@property BOOL isPlaceholder;

@property NSManagedObjectID *gameObjID;
@property (nullable, readonly) NSImage *image;

// Used as the intrinsic content size before macOS 10.14
@property NSSize intrinsic;

// The pasteboard types an image drop or paste is accepted from
@property (class, readonly) NSSet<NSPasteboardType> *acceptableDragTypes;

- (instancetype)initWithGame:(Game *)game image:(nullable NSImage *)anImage;
- (void)processImage:(NSImage *)image;
- (void)paste:(id)sender;

// For CoverImageView
@property (readonly) CGFloat ratio;
- (void)positionImagelayer;

@end

NS_ASSUME_NONNULL_END

//
//  ImageHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-03-30.
//
//  The pictures of the running game, by Glk resource number, and Glk's
//  one-slot "last image": FINDIMAGE, LOADIMAGE and PURGEIMG each make a
//  picture current, and DRAWIMAGE and SIZEIMAGE use whichever that is.
//
//  Main thread only, except for the Blorb parsing that
//  cacheImagesFromBlorbURL:withData: starts on a background queue. Every
//  other method waits for that to finish before it looks anything up.
//

#import <AppKit/AppKit.h>
#import "ImageAnimation.h"

NS_ASSUME_NONNULL_BEGIN

@interface ImageHandler : NSObject

// The current picture and its number, or nil and -1.
@property (readonly, nullable) NSImage *lastimage;
@property (readonly) NSInteger lastimageresno;

// Reads the pictures of the game's Blorb (the game file itself, or a .blb
// next to it) in the background.
- (void)cacheImagesFromBlorbURL:(NSURL *)file withData:(nullable NSData *)data;

#pragma mark Glk requests: these change the last image

// Makes picture resno current. Returns NO, leaving no current picture, if
// it is unknown or cannot be decoded.
- (BOOL)handleFindImageNumber:(NSInteger)resno;
// Declares picture resno as size bytes at offset in path, unless it is
// known already, and makes it current.
- (void)handleLoadImageNumber:(NSInteger)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                         size:(NSUInteger)size;
// Forgets picture resno. With a replacement, the whole file at path (a
// temporary file the handler then owns and deletes once it has decoded it)
// becomes picture resno and current.
- (void)purgeImage:(NSInteger)resno withReplacement:(nullable NSString *)path;

#pragma mark Lookups: these leave the last image alone

// So a rescale or an autorestore can ask about any picture in a window
// without changing what the next draw request draws.
- (nullable NSImage *)imageForNumber:(NSInteger)resno;
// The undecoded bytes of picture resno.
- (nullable NSData *)dataForImageNumber:(NSInteger)resno;
// The animation for picture resno, or nil if it is a still image.
- (nullable ImageAnimation *)animationForImageNumber:(NSInteger)resno;

@end

NS_ASSUME_NONNULL_END

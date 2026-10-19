//
//  ImageResource.h
//  Spatterlight
//
//  A picture a game has declared: where its bytes are (a file, an offset and
//  a length) and, once read, the bytes themselves. Nothing here is archived:
//  the image handler is rebuilt from the game file and the interpreter after
//  an autorestore.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageResource : NSObject

// A length of 0 means everything from offset to the end of the file.
- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;
// A resource whose bytes are already in memory, such as a Blorb picture.
- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset data:(NSData *)data;

@property (readonly, copy) NSString *filename;
@property (readonly) NSUInteger offset;
@property (readonly) NSUInteger length;
@property (readonly, nullable) NSData *data;

// The file is a temporary one the interpreter wrote for the host and left to
// it (a PURGEIMG replacement), so the handler deletes it: once it is decoded,
// or else when the number is purged again.
@property BOOL ownsFile;

// Reads data from the file. Returns NO, and leaves data as it was, if the
// file cannot be opened or nothing can be read from it.
- (BOOL)loadWithError:(NSError **)outError;
// A new image made from data, or nil if data is missing or not a picture.
- (nullable NSImage *)createImage;

@end

NS_ASSUME_NONNULL_END

//
//  ImageHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-03-30.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageFile : NSObject

- (instancetype)initWithPath:(NSString *)path;
- (instancetype)initWithURL:(NSURL *)path;
- (void)resolveBookmark;

@property (nullable) NSData *bookmark;
@property (nullable) NSURL *URL;

@end


@interface ImageResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL load NS_SWIFT_UNAVAILABLE("Use the throwing method instead");
- (BOOL)loadWithError:(NSError**)outError;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSImage * _Nullable createImage;

@property (nullable) NSData *data;
@property (nullable) ImageFile *imageFile;
@property NSString *filename;
@property NSUInteger offset;
@property NSUInteger length;
@property NSString *a11yDescription;

@end


@interface ImageHandler : NSObject

@property NSCache<NSNumber *, NSImage *> *imageCache;

@property NSInteger lastimageresno;
@property (nullable) NSImage *lastimage;
@property NSMutableDictionary <NSNumber *, ImageResource *> *resources;
@property NSMutableDictionary <NSString *, ImageFile *> *files;

- (void)cacheImagesFromBlorb:(NSURL *)file;
@property (readonly, nonatomic, copy) NSString *lastImageLabel;

- (BOOL)handleFindImageNumber:(NSInteger)resno;
- (void)handleLoadImageNumber:(NSInteger)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       size:(NSUInteger)size;
- (void)purgeImage:(NSInteger)resno withReplacement:(nullable NSString *)path
            size:(NSUInteger)size;

@end

NS_ASSUME_NONNULL_END

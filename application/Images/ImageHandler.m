//
//  ImageHandler.m
//  Spatterlight
//
//  Created by Administrator on 2021-03-30.
//

#import "ImageHandler.h"
#import "ImageResource.h"
#import "DebugLog.h"
#import <BlorbFramework/BlorbFramework.h>
#import <BlorbFramework/BlorbFramework-Swift.h>

// Byte budget for the decoded pictures the NSCache keeps alive. An NSImage
// is cheap until it draws; after that its bitmap rep holds the decoded pixels
// (4 bytes per pixel), which is what this limit counts. Kerkerkruip 9 is the
// heaviest blorb seen: its intro animation cycles 115 frames of 500x279
// (64 MiB decoded) and all 452 of its pictures decode to 178 MiB, so this
// holds a whole game of that size and the cache starts evicting only beyond.
static const NSUInteger kImageCacheCostLimit = 256 * 1024 * 1024;

@implementation ImageHandler {
    NSMutableDictionary<NSNumber *, ImageResource *> *_resources;
    // The Blorb's alt texts. Kept apart from the resources because a
    // purged picture's replacement still has its number's description.
    NSMutableDictionary<NSNumber *, NSString *> *_imageDescriptions;
    NSCache<NSNumber *, NSImage *> *_imageCache;
    // Signals when the initial background caching of Blorb images (kicked off by
    // cacheImagesFromBlorbURL:) has finished. Main-thread readers wait on this
    // before touching _resources/_imageDescriptions so they never race the
    // caching queue. A group that was never entered returns from wait
    // immediately.
    dispatch_group_t _cacheGroup;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _resources = [NSMutableDictionary new];
        _imageDescriptions = [NSMutableDictionary new];
        _lastimageresno = -1;
        _imageCache = [NSCache new];
        _imageCache.totalCostLimit = kImageCacheCostLimit;
        _cacheGroup = dispatch_group_create();
    }
    return self;
}

// A replacement file is left in the interpreter's temp folder for the host
// to delete, and nothing else reads it.
- (void)deleteFileOfResource:(ImageResource *)resource {
    if (!resource.ownsFile)
        return;
    resource.ownsFile = NO;
    NSError *error = nil;
    if (![[NSFileManager defaultManager] removeItemAtPath:resource.filename error:&error])
        NSLog(@"ImageHandler: could not delete %@: %@", resource.filename, error);
}

// Store an image in the cache with its decoded size as the cost, so
// totalCostLimit bounds memory rather than object count.
- (void)cacheImage:(NSImage *)image forResource:(NSInteger)resno {
    NSUInteger cost = 0;
    for (NSImageRep *rep in image.representations) {
        NSInteger w = rep.pixelsWide, h = rep.pixelsHigh;
        if (w <= 0 || h <= 0) {
            w = (NSInteger)ceil(rep.size.width);
            h = (NSInteger)ceil(rep.size.height);
        }
        cost += (NSUInteger)(w * h) * 4;
    }
    [_imageCache setObject:image forKey:@(resno) cost:cost];
}

// The image for resno: the last one found, a cached one, or one decoded from
// its resource (read from its file first if need be) and cached here. Does
// not touch the "last image".
- (nullable NSImage *)imageForResource:(NSInteger)resno {
    if (_lastimageresno == resno && _lastimage)
        return _lastimage;
    NSImage *image = [_imageCache objectForKey:@(resno)];
    if (image)
        return image;
    ImageResource *resource = _resources[@(resno)];
    if (!resource || (!resource.data && ![resource loadWithError:NULL]))
        return nil;
    image = [resource createImage];
    if (!image)
        return nil;
    image.accessibilityDescription = _imageDescriptions[@(resno)];
    // This is the path every blorb-resident picture takes: the resource
    // table is filled at launch, so the FINDIMAGE request lands here and
    // the LOADIMAGE request never follows. Store here, or each draw
    // rebuilds the NSImage and decodes the data again.
    [self cacheImage:image forResource:resno];
    return image;
}

// Makes the image for resno the last image, or clears the last image if
// there is none. Returns whether there is one.
- (BOOL)selectImageNumber:(NSInteger)resno {
    _lastimage = [self imageForResource:resno];
    _lastimageresno = _lastimage ? resno : -1;
    return _lastimage != nil;
}

- (void)cacheImagesFromBlorbData:(NSData *)blorbData path:(NSString *)path {
    Blorb *blorb = [[Blorb alloc] initWithData:blorbData];
    for (BlorbResource *res in [blorb resourcesForUsage:PictureResource]) {
        NSData *data = [blorb dataForResource:res];
        if (!data)
            continue;
        _resources[@(res.number)] = [[ImageResource alloc] initWithFilename:path offset:res.start data:data];
        _imageDescriptions[@(res.number)] = res.descriptiontext;
    }
}

// Parse and cache the game's images off the main thread. Blorb parsing plus the
// per-picture data extraction is proportional to the number and size of images
// in the game file and used to block the launch critical path. It now runs on a
// background queue while the window is laid out and the interpreter boots; the
// group lets image-request handlers wait for it if a request somehow beats it.
- (void)cacheImagesFromBlorbURL:(NSURL *)file withData:(nullable NSData *)blorbData {
    dispatch_group_enter(_cacheGroup);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [self performCacheImagesFromBlorbURL:file withData:blorbData];
        dispatch_group_leave(self->_cacheGroup);
    });
}

- (void)performCacheImagesFromBlorbURL:(NSURL *)file withData:(nullable NSData *)blorbData {
    if ([Blorb isBlorbURL:file]) {
        if (blorbData)
            [self cacheImagesFromBlorbData:blorbData path:file.path];
        return;
    }

    // A game that is not a Blorb may have its pictures in one next to it.
    NSString *newPath = [file.path.stringByDeletingPathExtension stringByAppendingPathExtension:@"blb"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:newPath])
        return;
    NSFileCoordinator *coordinator = [NSFileCoordinator new];
    NSError *error = nil;
    [coordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:newPath isDirectory:NO]
                                    options:NSFileCoordinatorReadingWithoutChanges
                                      error:&error
                                 byAccessor:^(NSURL * _Nonnull newURL) {
        NSData *data = [NSData dataWithContentsOfURL:newURL];
        if (data)
            [self cacheImagesFromBlorbData:data path:newURL.path];
    }];
    if (error)
        NSLog(@"ImageHandler: could not read %@: %@", newPath, error);
}

// Block until the background caching started by cacheImagesFromBlorbURL: has
// finished, so callers on the main thread see a fully populated _resources and
// never mutate it concurrently with the caching queue. Returns immediately once
// caching is done (or was never started).
- (void)waitForCaching {
    dispatch_group_wait(_cacheGroup, DISPATCH_TIME_FOREVER);
}

#pragma mark Glk request calls from GlkController

- (BOOL)handleFindImageNumber:(NSInteger)resno {
    [self waitForCaching];
    return [self selectImageNumber:resno];
}

- (void)handleLoadImageNumber:(NSInteger)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                         size:(NSUInteger)size {
    [self waitForCaching];
    // A picture already read (a Blorb one, say) is kept as it is.
    if (!_resources[@(resno)].data)
        _resources[@(resno)] = [[ImageResource alloc] initWithFilename:path offset:offset length:size];
    [self selectImageNumber:resno];
}

- (void)purgeImage:(NSInteger)resno withReplacement:(nullable NSString *)replacementPath {
    if (resno < 0)
        return;

    [self waitForCaching];

    [_imageCache removeObjectForKey:@(resno)];
    [self deleteFileOfResource:_resources[@(resno)]];
    [_resources removeObjectForKey:@(resno)];
    _lastimage = nil;
    _lastimageresno = -1;
    if (!replacementPath)
        return;

    ImageResource *replacement = [[ImageResource alloc] initWithFilename:replacementPath offset:0 length:0];
    replacement.ownsFile = YES;
    _resources[@(resno)] = replacement;
    // The picture is decoded from the data read, so once that worked the
    // file can go. glkimp removes its temp folder when the interpreter
    // exits, but until then the last replacement under every number would
    // stay there: 6 to 20 MB over the walkthrough of a V6 game.
    if ([self selectImageNumber:resno])
        [self deleteFileOfResource:replacement];
}

#pragma mark Lookups

- (nullable NSImage *)imageForNumber:(NSInteger)resno {
    [self waitForCaching];
    return [self imageForResource:resno];
}

- (nullable NSData *)dataForImageNumber:(NSInteger)resno {
    [self waitForCaching];
    ImageResource *resource = _resources[@(resno)];
    if (!resource.data)
        [resource loadWithError:NULL];
    return resource.data;
}

- (nullable ImageAnimation *)animationForImageNumber:(NSInteger)resno {
    return [ImageAnimation animationForImage:[self imageForNumber:resno]];
}

@end

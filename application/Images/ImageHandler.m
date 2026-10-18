//
//  ImageHandler.m
//  Spatterlight
//
//  Created by Administrator on 2021-03-30.
//

#import "ImageHandler.h"
#import <BlorbFramework/BlorbFramework.h>
#import <BlorbFramework/BlorbFramework-Swift.h>
#import <objc/runtime.h>

static const void *kImageAnimationKey = &kImageAnimationKey;

@implementation ImageAnimation {
    NSBitmapImageRep *_rep;
    // End time of each frame, in seconds from the start of a pass.
    NSTimeInterval *_frameEnds;
}

// The first bitmap rep of image that has more than one frame, or nil.
static NSBitmapImageRep *animatedRep(NSImage *image) {
    for (NSImageRep *rep in image.representations) {
        if (![rep isKindOfClass:[NSBitmapImageRep class]])
            continue;
        NSNumber *frames = [(NSBitmapImageRep *)rep valueForProperty:NSImageFrameCount];
        if (frames.unsignedIntegerValue > 1)
            return (NSBitmapImageRep *)rep;
    }
    return nil;
}

+ (nullable ImageAnimation *)animationForImage:(NSImage *)image {
    if (!image)
        return nil;
    // Kept as an associated object so that it goes when the image goes: the
    // image cache's cost limit then bounds animations along with stills, and
    // every cell that draws the image gets the same instance.
    ImageAnimation *animation = objc_getAssociatedObject(image, kImageAnimationKey);
    if (animation)
        return animation == (id)[NSNull null] ? nil : animation;
    NSBitmapImageRep *rep = animatedRep(image);
    animation = rep ? [[ImageAnimation alloc] initWithImage:image rep:rep] : nil;
    objc_setAssociatedObject(image, kImageAnimationKey, animation ?: [NSNull null],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return animation;
}

- (instancetype)initWithImage:(NSImage *)image rep:(NSBitmapImageRep *)rep {
    self = [super init];
    if (self) {
        _image = image;
        _rep = rep;
        _frameCount = ((NSNumber *)[rep valueForProperty:NSImageFrameCount]).unsignedIntegerValue;

        // A GIF without a looping extension plays once.
        _loopCount = 1;
        NSNumber *loops = [rep valueForProperty:NSImageLoopCount];
        if (loops)
            _loopCount = loops.unsignedIntegerValue;

        // Each frame's delay is only readable while it is the current frame.
        NSNumber *current = [rep valueForProperty:NSImageCurrentFrame];
        _frameEnds = calloc(_frameCount, sizeof(NSTimeInterval));
        for (NSUInteger i = 0; i < _frameCount; i++) {
            [rep setProperty:NSImageCurrentFrame withValue:@(i)];
            NSTimeInterval seconds = ((NSNumber *)[rep valueForProperty:NSImageCurrentFrameDuration]).doubleValue;
            // Like web browsers, treat delays too short to display as 100 ms.
            if (seconds < 0.011)
                seconds = 0.1;
            _duration += seconds;
            _frameEnds[i] = _duration;
        }
        [rep setProperty:NSImageCurrentFrame withValue:current ?: @0];
    }
    return self;
}

- (void)dealloc {
    free(_frameEnds);
}

- (NSUInteger)frameAtTime:(NSTimeInterval)elapsed
                untilNext:(nullable NSTimeInterval *)untilNext {
    if (elapsed < 0)
        elapsed = 0;
    if (_loopCount && elapsed >= _duration * _loopCount) {
        if (untilNext)
            *untilNext = INFINITY;
        return _frameCount - 1;
    }
    NSTimeInterval position = fmod(elapsed, _duration);
    NSUInteger frame = 0;
    while (frame < _frameCount - 1 && _frameEnds[frame] <= position)
        frame++;
    if (untilNext)
        *untilNext = _frameEnds[frame] - position;
    return frame;
}

- (nullable NSImage *)imageForFrame:(NSUInteger)frame {
    if (frame >= _frameCount)
        return nil;
    [_rep setProperty:NSImageCurrentFrame withValue:@(frame)];
    return _image;
}

@end

@implementation ImageFile : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithPath:(NSString *)path {
    return [self initWithURL:[NSURL fileURLWithPath:path isDirectory:NO]];
}

- (instancetype)initWithURL:(NSURL *)path {
    self = [super init];
    if (self) {
        _URL = path;
        NSError *theError;
        _bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                        includingResourceValuesForKeys:nil
                                         relativeToURL:nil
                                                 error:&theError];
        if (theError || !_bookmark)
            NSLog(@"Imagefile error when encoding bookmark %@: %@", path, theError);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    _URL =  [decoder decodeObjectOfClass:[NSURL class] forKey:@"URL"];
    _bookmark = [decoder decodeObjectOfClass:[NSData class] forKey:@"bookmark"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_URL forKey:@"URL"];
    [encoder encodeObject:_bookmark forKey:@"bookmark"];
}

- (void)resolveBookmark {
    BOOL bookmarkIsStale = NO;
    NSError *error = nil;
    _URL = [NSURL URLByResolvingBookmarkData:_bookmark options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:&bookmarkIsStale error:&error];
    if (bookmarkIsStale) {
        _bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                        includingResourceValuesForKeys:nil
                                         relativeToURL:nil
                                                 error:&error];
    }
    if (error) {
        NSLog(@"Imagefile resolveBookmark: %@", error);
    }
}

@end


@implementation ImageResource : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithFilename:(NSString *)filename  offset:(NSUInteger)offset length:(NSUInteger)length {
    self = [super init];
    if (self) {
        _filename = filename;
        _offset = offset;
        _length = length;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    _filename = [decoder decodeObjectOfClass:[NSString class] forKey:@"filename"];
    _length = (size_t)[decoder decodeIntegerForKey:@"length"];
    _offset = (size_t)[decoder decodeIntegerForKey:@"offset"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_filename forKey:@"filename"];
    [encoder encodeInteger:(NSInteger)_length forKey:@"length"];
    [encoder encodeInteger:(NSInteger)_offset forKey:@"offset"];
}

-(BOOL)load {
    return [self loadWithError:NULL];
}

- (BOOL)loadWithError:(NSError* __autoreleasing *)outError {
    NSFileHandle *fileHandle = [NSFileHandle fileHandleForReadingFromURL:_imageFile.URL error:outError];
    if (!fileHandle) {
        [_imageFile resolveBookmark];
        fileHandle = [NSFileHandle fileHandleForReadingFromURL:_imageFile.URL error:outError];
        if (!fileHandle)
            return NO;
    }
    [fileHandle seekToFileOffset:_offset];
    _data = [fileHandle readDataOfLength:_length];
    if (!_data) {
        NSLog(@"Could not read image resource from file %@, length %ld, offset %ld\n", _imageFile.URL.path, _length, _offset);
        if (outError) {
            *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:@{
                NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Could not read image resource from file %@, length %ld, offset %ld\n", _imageFile.URL.path, _length, _offset]}
            ];
        }
        return NO;
    }

    return YES;
}

- (nullable NSImage *)createImage {

    if (!_data)
        return nil;

    NSArray *reps = [NSBitmapImageRep imageRepsWithData:_data];
    if (reps.count == 0) {
        if (_filename.length && _offset == 0) {
            NSImage *dummy = [[NSImage alloc] initWithContentsOfFile:_filename];
            reps = dummy.representations;
        }
        if (reps.count == 0)
            return nil;
    }
    NSImageRep *rep = reps[0];
    NSSize size = NSMakeSize((CGFloat)rep.pixelsWide, (CGFloat)rep.pixelsHigh);

    if (size.height == 0 || size.width == 0) {
        NSLog(@"ImageResource: image size is zero!");
        return nil;
    }

    NSImage *img = [[NSImage alloc] initWithSize:size];

    if (!img) {
        NSLog(@"ImageResource: failed to decode image");
        return nil;
    }

    [img addRepresentations:reps];
    img.accessibilityDescription = _a11yDescription;
    return img;
}

@end

// Byte budget for the decoded pictures the NSCache keeps alive. An NSImage
// is cheap until it draws; after that its bitmap rep holds the decoded pixels
// (4 bytes per pixel), which is what this limit counts. Kerkerkruip 9 is the
// heaviest blorb seen: its intro animation cycles 115 frames of 500x279
// (64 MiB decoded) and all 452 of its pictures decode to 178 MiB, so this
// holds a whole game of that size and the cache starts evicting only beyond.
static const NSUInteger kImageCacheCostLimit = 256 * 1024 * 1024;

@implementation ImageHandler {
    // Signals when the initial background caching of Blorb images (kicked off by
    // cacheImagesFromBlorbURL:) has finished. Main-thread readers wait on this
    // before touching _resources/_imageDescriptions so they never race the
    // caching queue. A group that was never entered returns from wait
    // immediately, so the decoded-from-autosave case needs no special handling.
    dispatch_group_t _cacheGroup;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _resources = [NSMutableDictionary new];
        _files = [NSMutableDictionary new];
        _lastimageresno = -1;
        _imageCache = [NSCache new];
        _imageCache.totalCostLimit = kImageCacheCostLimit;
        _imageDescriptions = [NSMutableDictionary new];
        _cacheGroup = dispatch_group_create();
    }
    return self;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    _files = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"files"];
    _resources = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"resources"];
    if (_resources)
        for (ImageResource *res in _resources.allValues) {
            res.imageFile = _files[res.filename];
        }
    _lastimageresno = [decoder decodeIntForKey:@"lastimageresno"];
    _cacheGroup = dispatch_group_create();
    // The cache is not archived (it is only decoded pixels), and a restored
    // handler needs one just as much as a fresh one.
    _imageCache = [NSCache new];
    _imageCache.totalCostLimit = kImageCacheCostLimit;
    _imageDescriptions = [NSMutableDictionary new];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_files forKey:@"files"];
    [encoder encodeObject:_resources forKey:@"resources"];
    [encoder encodeInteger:_lastimageresno forKey:@"lastimageresno"];
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
// its resource and cached here. Does not touch the "last image".
- (nullable NSImage *)imageForResource:(NSInteger)resno {
    if (_lastimageresno == resno && _lastimage)
        return _lastimage;
    NSImage *image = [_imageCache objectForKey:@(resno)];
    if (image)
        return image;
    ImageResource *resource = _resources[@(resno)];
    if (!resource)
        return nil;
    image = [resource createImage];
    // This is the path every blorb-resident picture takes: the resource
    // table is filled at launch, so the FINDIMAGE request lands here and
    // the LOADIMAGE request (whose handler stores into the cache) never
    // follows. Store here too, or the cache stays empty and each draw
    // rebuilds the NSImage and decodes the data again.
    if (image)
        [self cacheImage:image forResource:resno];
    return image;
}

- (BOOL)imageIsLoaded:(NSInteger)resno {
    NSImage *image = [self imageForResource:resno];
    if (!image)
        return NO;
    _lastimage = image;
    _lastimageresno = resno;
    return YES;
}

- (void)cacheImagesFromBlorbData:(NSData *)blorbData path:(NSString *)path {
    Blorb *blorb = [[Blorb alloc] initWithData:blorbData];
    NSArray *resources = [blorb resourcesForUsage:PictureResource];
    for (BlorbResource *res in resources) {
        NSInteger resno = res.number;
        NSData *data = [blorb dataForResource:res];
        ImageResource *imgres = [[ImageResource alloc] initWithFilename:path offset:res.start length:data.length];
        imgres.data = data;
        imgres.a11yDescription = res.descriptiontext;
        _resources[@(resno)] = imgres;
        _imageDescriptions[@(resno)] = res.descriptiontext;
        //        if (res.descriptiontext)
        //            NSLog(@"Cached image %ld with path: %@ offset: %u length: %ld text:%@", resno, file.path, res.start, data.length, res.descriptiontext);
    }
}

// Parse and cache the game's images off the main thread. Blorb parsing plus the
// per-picture data extraction is proportional to the number and size of images
// in the game file and used to block the launch critical path. It now runs on a
// background queue while the window is laid out and the interpreter boots; the
// group lets image-request handlers wait for it if a request somehow beats it.
- (void)cacheImagesFromBlorbURL:(NSURL *)file withData:(NSData *)blorbData {
    dispatch_group_enter(_cacheGroup);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [self performCacheImagesFromBlorbURL:file withData:blorbData];
        dispatch_group_leave(self->_cacheGroup);
    });
}

- (void)performCacheImagesFromBlorbURL:(NSURL *)file withData:(NSData *)blorbData {
    if (![Blorb isBlorbURL:file]) {
        NSString *newPath = [[file.path stringByDeletingPathExtension] stringByAppendingPathExtension:@"blb"];
        file = [NSURL fileURLWithPath:newPath isDirectory:NO];

        NSFileCoordinator *coordinator = [NSFileCoordinator new];

        NSError *error = nil;

        [coordinator coordinateReadingItemAtURL:file options:NSFileCoordinatorReadingWithoutChanges error:&error byAccessor:^(NSURL * _Nonnull newURL) {
            [self cacheImagesFromBlorbData:[NSData dataWithContentsOfURL:newURL] path:newURL.path];
        }];
    } else {
        [self cacheImagesFromBlorbData:blorbData path:file.path];
    }
}

// Block until the background caching started by cacheImagesFromBlorbURL: has
// finished, so callers on the main thread see a fully populated _resources and
// never mutate it concurrently with the caching queue. Returns immediately once
// caching is done (or was never started, as after autosave restore).
- (void)waitForCaching {
    dispatch_group_wait(_cacheGroup, DISPATCH_TIME_FOREVER);
}

#pragma mark Glk request calls from GlkController

- (BOOL)handleFindImageNumber:(NSInteger)resno {
    [self waitForCaching];
    BOOL result = [self imageIsLoaded:resno];

    if (!result) {
        [_resources[@(resno)] loadWithError:NULL];
        result = [self imageIsLoaded:resno];
    }
    if (!result)
        _lastimageresno = -1;
    else
        _lastimageresno = resno;
    return result;
}

- (void)handleLoadImageNumber:(NSInteger)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       size:(NSUInteger)size {

    [self waitForCaching];
    if ([self imageIsLoaded:resno])
        return;

    [self setImageID:resno filename:path size:size offset:offset];
    if (size == 8) {
        // Hack for placeholder images, which only have dimensions, no content.
        NSInteger width = ((const unsigned char *)(_resources[@(resno)].data.bytes))[3] + ((const unsigned char *)(_resources[@(resno)].data.bytes))[2] * 0x100;
        NSInteger height = ((const unsigned char *)(_resources[@(resno)].data.bytes))[7] + ((const unsigned char *)(_resources[@(resno)].data.bytes))[6] * 0x100;

        // No size must be 0, or both will be, so we add a "rounding error"
        _lastimage = [[NSImage alloc] initWithSize:NSMakeSize((CGFloat)width + 0.01, (CGFloat)height + 0.01)];
    } else
        _lastimage = [_resources[@(resno)] createImage];
    if (_lastimage == nil) {
        _lastimageresno = -1;
        return;
    }
    [self cacheImage:_lastimage forResource:resno];
    _lastimageresno = resno;
}

- (void)setImageID:(NSInteger)resno filename:(nullable NSString *)filename size:(NSUInteger)length offset:(NSUInteger)offset {

    ImageResource *res = _resources[@(resno)];

    if (res == nil) {
        res = [[ImageResource alloc] initWithFilename:filename offset:offset length:length];
        _resources[@(resno)] = res;
    } else if (res.data) {
        return;
    }

    res.length = length;

    if (filename != nil)
    {
        res.imageFile = _files[filename];
        if (!res.imageFile) {
            res.imageFile = [[ImageFile alloc] initWithPath:filename];
            _files[filename] = res.imageFile;
        }
    } else return;
    res.offset = offset;
    [res loadWithError:NULL];
}

// Unlike handleFindImageNumber: this leaves the "last image" alone, so a
// rescale or an autorestore can ask about any image in the buffer without
// changing what the next draw request draws.
- (nullable ImageAnimation *)animationForImageNumber:(NSInteger)resno {
    [self waitForCaching];
    return [ImageAnimation animationForImage:[self imageForResource:resno]];
}

- (NSString *)lastImageLabel {
    NSString *label =@"";
    if (_lastimageresno != -1) {
        label = _resources[@(_lastimageresno)].a11yDescription;
    }
    return label;
}

- (void)purgeImage:(NSInteger)resno withReplacement:(nullable NSString *)replacementPath size:(NSUInteger)replacementSize {
    if (resno < 0)
        return;

    [self waitForCaching];

    [_imageCache removeObjectForKey:@(resno)];
    ImageResource *resource = _resources[@(resno)];
    NSError *error = nil;
    if (![Blorb isBlorbURL:resource.imageFile.URL]) {
        [[NSFileManager defaultManager] removeItemAtURL:resource.imageFile.URL error:&error];
        if (error) {
            NSLog(@"purgeImage: %@", error);
        }
    }
    _resources[@(resno)] = nil;

    if (!replacementPath) {
        _lastimageresno = -1;
        _lastimage = nil;
    } else {
        [self setImageID:resno filename:replacementPath size:replacementSize offset:0];
        _lastimage = [_resources[@(resno)] createImage];
        if (_lastimage == nil) {
            _lastimageresno = -1;
            return;
        }
        if (_lastimage.accessibilityDescription.length == 0) {
            _lastimage.accessibilityDescription = _imageDescriptions[@(resno)];
        }
        [self cacheImage:_lastimage forResource:resno];
        _lastimageresno = resno;
    }
}

@end

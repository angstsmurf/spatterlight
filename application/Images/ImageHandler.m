//
//  ImageHandler.m
//  Spatterlight
//
//  Created by Administrator on 2021-03-30.
//

#import "ImageHandler.h"
#import <BlorbFramework/BlorbFramework.h>

@implementation ImageFile : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithPath:(NSString *)path {
    self = [super init];
    if (self) {
        _URL = [NSURL fileURLWithPath:path];
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
    _URL =  [decoder decodeObjectOfClass:[NSURL class] forKey:@"URL"];
    _bookmark = [decoder decodeObjectOfClass:[NSData class] forKey:@"bookmark"];
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
    _filename = [decoder decodeObjectOfClass:[NSString class] forKey:@"filename"];
    _length = (size_t)[decoder decodeIntForKey:@"length"];
    _offset = (size_t)[decoder decodeIntForKey:@"offset"];
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_filename forKey:@"filename"];
    [encoder encodeInteger:(NSInteger)_length forKey:@"length"];
    [encoder encodeInteger:(NSInteger)_offset forKey:@"offset"];
}

-(BOOL)load {
    NSFileHandle *fileHandle = [NSFileHandle fileHandleForReadingAtPath:_imageFile.URL.path];
    if (!fileHandle) {
        [_imageFile resolveBookmark];
        fileHandle = [NSFileHandle fileHandleForReadingAtPath:_imageFile.URL.path];
        if (!fileHandle)
            return NO;
    }
    [fileHandle seekToFileOffset:_offset];
    _data = [fileHandle readDataOfLength:_length];
    if (!_data) {
        NSLog(@"Could not read image resource from file %@, length %ld, offset %ld\n", _imageFile.URL.path, _length, _offset);
        return NO;
    }

    return YES;
}

- (nullable NSImage *)createImage {

    if (!_data)
        return nil;

    NSArray *reps = [NSBitmapImageRep imageRepsWithData:_data];
    NSImageRep *rep = reps[0];
    NSSize size = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);

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

@implementation ImageHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _resources = [NSMutableDictionary new];
        _files = [NSMutableDictionary new];
        _lastimageresno = -1;
        _imageCache = [NSCache new];
    }
    return self;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _files = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"files"];
    _resources = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"resources"];
    if (_resources)
        for (ImageResource *res in _resources.allValues) {
            res.imageFile = _files[res.filename];
        }
    _lastimageresno = [decoder decodeIntForKey:@"lastimageresno"];
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_files forKey:@"files"];
    [encoder encodeObject:_resources forKey:@"resources"];
    [encoder encodeInteger:_lastimageresno forKey:@"lastimageresno"];
}

- (BOOL)imageIsLoaded:(NSInteger)resno {
    if (_lastimageresno == resno && _lastimage) {
        return YES;
    }
    NSImage *cachedimage = [_imageCache objectForKey:@(resno)];
    if (cachedimage) {
        _lastimage = cachedimage;
        _lastimageresno = resno;
//        NSLog(@"Image found in cache!");
        return YES;
    }
    ImageResource *resource = _resources[@(resno)];
    if (resource) {
        _lastimage = [resource createImage];
        return (_lastimage != nil);
    }
    return NO;
}

- (void)cacheImagesFromBlorb:(NSURL *)file {
    if (![Blorb isBlorbURL:file])
        return;
    Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:file]];
    NSArray *resources = [blorb resourcesForUsage:PictureResource];
    for (BlorbResource *res in resources) {
        NSInteger resno = res.number;
        NSData *data = [blorb dataForResource:res];
        ImageResource *imgres = [[ImageResource alloc] initWithFilename:file.path offset:res.start length:data.length];
        imgres.data = data;
        imgres.a11yDescription = res.descriptiontext;
        _resources[@(resno)] = imgres;
//        if (res.descriptiontext)
//            NSLog(@"Cached image %ld with path: %@ offset: %u length: %ld text:%@", resno, file.path, res.start, data.length, res.descriptiontext);
    }
}

#pragma mark Glk request calls from GlkController

- (BOOL)handleFindImageNumber:(NSInteger)resno {
    BOOL result = [self imageIsLoaded:resno];

    if (!result) {
        [_resources[@(resno)] load];
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
                       length:(NSUInteger)length {

    if ([self imageIsLoaded:resno])
        return;

    [self setImageID:resno filename:path length:length offset:offset];
    _lastimage = [_resources[@(resno)] createImage];
    [_imageCache setObject:_lastimage forKey:@(resno)];
    _lastimageresno = resno;
}

- (void)setImageID:(NSInteger)resno filename:(nullable NSString *)filename length:(NSUInteger)length offset:(NSUInteger)offset {

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
    [res load];
}

- (NSString *)lastImageLabel {
    NSString *label =@"";
    if (_lastimageresno != -1) {
        label = _resources[@(_lastimageresno)].a11yDescription;
    }
    return label;
}

@end

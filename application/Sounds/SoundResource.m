//
//  SoundResource.m
//  Spatterlight
//

#import "SoundResource.h"

#import "SoundHandler.h"
#import "GlkController.h"
#import "NSData+Categories.h"

@implementation SoundFile : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithPath:(NSString *)path {
    self = [super init];
    if (self) {
        _URL = [NSURL fileURLWithPath:path isDirectory:NO];
        NSError *theError;
        _bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                   includingResourceValuesForKeys:nil
                                    relativeToURL:nil
                                            error:&theError];
        if (theError || !_bookmark)
            NSLog(@"Soundfile error when encoding bookmark %@: %@", path, theError);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    if (self = [super init]) {
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
    if (!_bookmark)
        return;
    BOOL bookmarkIsStale = NO;
    NSError *error = nil;
    NSURL *resolved = [NSURL URLByResolvingBookmarkData:_bookmark options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:&bookmarkIsStale error:&error];
    if (resolved) {
        _URL = resolved;
        if (bookmarkIsStale)
            [self refreshBookmark];
        return;
    }

    NSLog(@"Soundfile resolveBookmark: %@", error);
    if (error.code != NSFileNoSuchFileError)
        return;

    /* The file has gone from where it was. Look for one of the same name next
       to the game file, which is where a moved game takes its Blorb along. */
    NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                                       fromBookmarkData:_bookmark];
    NSString *oldfilename = [values[NSURLPathKey] lastPathComponent];
    NSString *gamefile = _handler.glkctl.gamefile;
    if (!oldfilename.length || !gamefile.length)
        return;
    NSString *newPath =
    [gamefile.stringByDeletingLastPathComponent stringByAppendingPathComponent:oldfilename];
    if ([[NSFileManager defaultManager] fileExistsAtPath:newPath]) {
        _URL = [NSURL fileURLWithPath:newPath isDirectory:NO];
        [self refreshBookmark];
    }
}

- (void)refreshBookmark {
    NSError *error = nil;
    NSData *bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                      includingResourceValuesForKeys:nil
                                       relativeToURL:nil
                                               error:&error];
    if (bookmark)
        _bookmark = bookmark;
    else
        NSLog(@"Soundfile error when refreshing bookmark %@: %@", _URL.path, error);
}

@end


@implementation SoundResource : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length {
    self = [super init];
    if (self) {
        _filename = filename;
        _offset = offset;
        _length = length;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    if (self = [super init]) {
        _filename = [decoder decodeObjectOfClass:[NSString class] forKey:@"filename"];
        _length = (NSUInteger)[decoder decodeIntegerForKey:@"length"];
        _offset = (NSUInteger)[decoder decodeIntegerForKey:@"offset"];
        _type = (GlkSoundBlorbFormatType)[decoder decodeIntegerForKey:@"type"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_filename forKey:@"filename"];
    [encoder encodeInteger:(NSInteger)_length forKey:@"length"];
    [encoder encodeInteger:(NSInteger)_offset forKey:@"offset"];
    [encoder encodeInteger:(NSInteger)_type forKey:@"type"];
}

-(BOOL)load {
    if (!_soundFile.URL)
        [_soundFile resolveBookmark];
    NSError *error = nil;
    NSData *data = [NSData dataFromFileURL:_soundFile.URL offset:_offset length:_length error:&error];
    if (!data) {
        [_soundFile resolveBookmark];
        data = [NSData dataFromFileURL:_soundFile.URL offset:_offset length:_length error:&error];
    }

    if (data.length != _length) {
        NSLog(@"Could not read sound resource from file %@, length %lu, offset %lu (got %lu bytes) %@", _soundFile.URL.path, (unsigned long)_length, (unsigned long)_offset, (unsigned long)data.length, error ?: @"");
        return NO;
    }

    if (!_type) {
        _type = [SoundResource detectSoundFormatFromData:data];
        if (!_type) {
            NSLog(@"Unknown format");
            return NO;
        }
    }

    _data = data;
    return YES;
}

/* Sniff the format from the data. MP3 has no reliable magic of its own, so
   anything not recognised is taken to be MP3; only empty data is None. */
+ (GlkSoundBlorbFormatType)detectSoundFormatFromData:(NSData*)data {
    const char *buf = (const char *)data.bytes;
    const NSUInteger length = data.length;

    if (length == 0)
        return GlkSoundBlorbFormatNone;

    /* AIFF (IFF) */
    if (length >= 4 && !memcmp(buf, "FORM", 4))
        return GlkSoundBlorbFormatFORM;

    /* WAVE */
    if (length >= 4 && !memcmp(buf, "WAVE", 4))
        return GlkSoundBlorbFormatWave;

    if (length >= 4 && !memcmp(buf, "RIFF", 4))
        return GlkSoundBlorbFormatWave;

    /* midi */
    if (length >= 4 && !memcmp(buf, "MThd", 4))
        return GlkSoundBlorbFormatMIDI;

    /* s3m */
    if (length >= 0x30 && !memcmp(buf + 0x2c, "SCRM", 4))
        return GlkSoundBlorbFormatMod;

    /* XM */
    if (length >= 17 && !memcmp(buf, "Extended Module: ", 17))
        return GlkSoundBlorbFormatMod;

    /* MOD */
    if (length >= 1084) {
        const char *resname = buf + 1080;
        if (!memcmp(resname+1, "CHN", 3) ||        /* 4CHN, 6CHN, 8CHN */
            !memcmp(resname+2, "CN", 2) ||         /* 16CN, 32CN */
            !memcmp(resname, "M.K.", 4) || !memcmp(resname, "M!K!", 4) ||
            !memcmp(resname, "FLT4", 4) || !memcmp(resname, "CD81", 4) ||
            !memcmp(resname, "OKTA", 4) || !memcmp(resname, "    ", 4))
            return GlkSoundBlorbFormatMod;
    }

    /* IT */
    if (length >= 4 && !memcmp(buf, "IMPM", 4))
        return GlkSoundBlorbFormatMod;

    /* ogg */
    if (length >= 4 && !memcmp(buf, "OggS", 4))
        return GlkSoundBlorbFormatOggVorbis;

    return GlkSoundBlorbFormatMP3;
}

@end

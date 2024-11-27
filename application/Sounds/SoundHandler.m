//
//  SoundHandler.mm
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import "SoundHandler.h"

#import "GlkSoundChannel.h"
#import "MIDIChannel.h"
#import "GlkController.h"
#import "GlkEvent.h"

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
        NSLog(@"Soundfile resolveBookmark: %@", error);
        if (error.code == 4) {
            NSDictionary *values = [NSURL resourceValuesForKeys:@[NSURLPathKey]
                                               fromBookmarkData:_bookmark];
            NSString *oldfilename = values[NSURLPathKey];
            oldfilename = oldfilename.lastPathComponent;
            NSString *newPath =
            [(_handler.glkctl.gamefile).stringByDeletingLastPathComponent stringByAppendingPathComponent:oldfilename];
            if ([[NSFileManager defaultManager] fileExistsAtPath:newPath]) {
                _URL = [NSURL fileURLWithPath:newPath isDirectory:NO];
                _bookmark = [_URL bookmarkDataWithOptions:NSURLBookmarkCreationSuitableForBookmarkFile
                           includingResourceValuesForKeys:nil
                                            relativeToURL:nil
                                                    error:&error];
            }
        }
    }
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
        _length = (size_t)[decoder decodeIntForKey:@"length"];
        _offset = (size_t)[decoder decodeIntForKey:@"offset"];
        _type = (GlkSoundBlorbFormatType)[decoder decodeIntForKey:@"type"];
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
    NSFileHandle *fileHandle = [NSFileHandle fileHandleForReadingFromURL:_soundFile.URL error:NULL];
    if (!fileHandle) {
        [_soundFile resolveBookmark];
        fileHandle = [NSFileHandle fileHandleForReadingFromURL:_soundFile.URL error:NULL];
        if (!fileHandle)
            return NO;
    }
    [fileHandle seekToFileOffset:_offset];
    _data = [fileHandle readDataOfLength:_length];
    if (!_data) {
        NSLog(@"Could not read sound resource from file %@, length %ld, offset %ld\n", _soundFile.URL.path, _length, _offset);
        return NO;
    }

    if (!_type) {
        _type = [self detect_sound_format];
        if (!_type) {
            NSLog(@"Unknown format");
            return NO;
        }
    }

    return YES;
}

+ (GlkSoundBlorbFormatType)detectSoundFormatFromData:(NSData*)_data {
    const char *buf = (const char *)_data.bytes;
    const NSUInteger _length = _data.length;
    char str[30];
    if (_length > 29) {
        strncpy(str, buf, 29);
        str[29]='\0';
    } else {
        strncpy(str, buf, _length);
        str[_length - 1]='\0';
    }
    /* AIFF */
    if (_length > 4 && !memcmp(buf, "FORM", 4))
        return GlkSoundBlorbFormatFORM;

    /* WAVE */
    if (_length > 4 && !memcmp(buf, "WAVE", 4))
        return GlkSoundBlorbFormatWave;

    if (_length > 4 && !memcmp(buf, "RIFF", 4))
        return GlkSoundBlorbFormatWave;

    /* midi */
    if (_length > 4 && !memcmp(buf, "MThd", 4))
        return GlkSoundBlorbFormatMIDI;

    /* s3m */
    if (_length > 0x30 && !memcmp(buf + 0x2c, "SCRM", 4))
        return GlkSoundBlorbFormatMod;

    /* XM */
    if (_length > 20 && !memcmp(buf, "Extended Module: ", 17))
        return GlkSoundBlorbFormatMod;

    /* MOD */
    if (_length > 1084) {
        char resname[5];
        memcpy(resname, (buf) + 1080, 4);
        resname[4] = '\0';
        if (!strncmp(resname+1, "CHN", 3) ||        /* 4CHN, 6CHN, 8CHN */
            !strncmp(resname+2, "CN", 2) ||         /* 16CN, 32CN */
            !strncmp(resname, "M.K.", 4) || !strncmp(resname, "M!K!", 4) ||
            !strncmp(resname, "FLT4", 4) || !strncmp(resname, "CD81", 4) ||
            !strncmp(resname, "OKTA", 4) || !strncmp(resname, "    ", 4))
            return GlkSoundBlorbFormatMod;
    }

    /* ogg */
    if (_length > 4 && !memcmp(buf, "OggS", 4))
        return GlkSoundBlorbFormatOggVorbis;

    return GlkSoundBlorbFormatMP3;
}

- (GlkSoundBlorbFormatType)detect_sound_format {
    return [[self class] detectSoundFormatFromData:_data];
}

@end

@implementation SoundHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _resources = [NSMutableDictionary new];
        _sfbplayers = [NSMutableDictionary new];
        _glkchannels = [NSMutableDictionary new];
        _files = [NSMutableDictionary new];
        _lastsoundresno = -1;
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
        for (SoundFile *file in _files.allValues)
            file.handler = self;
        _resources = _resources = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [SoundResource class], nil] forKey:@"resources"];
        if (_resources)
            for (SoundResource *res in _resources.allValues) {
                res.soundFile = _files[res.filename];
            }
        _glkchannels = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [GlkSoundChannel class], nil] forKey:@"gchannels"];
        _lastsoundresno = [decoder decodeIntForKey:@"lastsoundresno"];
    }
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_files forKey:@"files"];
    [encoder encodeObject:_resources forKey:@"resources"];
    [encoder encodeObject:_glkchannels forKey:@"gchannels"];
    [encoder encodeInt:_lastsoundresno forKey:@"lastsoundresno"];
}

- (BOOL)soundIsLoaded:(glsi32)soundId {
    SoundResource *resource = _resources[@(soundId)];
    if (resource)
        return (resource.data != nil);
    return NO;
}

+ (NSString *)MIMETypeFromFormatType:(GlkSoundBlorbFormatType)format {
    NSString *mimeString;
    switch (format) {
        case GlkSoundBlorbFormatFORM:
        case GlkSoundBlorbFormatAIFF:
            mimeString = @"aiff";
            break;
        case GlkSoundBlorbFormatWave:
            mimeString = @"wav";
            break;
        case GlkSoundBlorbFormatOggVorbis:
            mimeString = @"ogg-vorbis";
            break;
        case GlkSoundBlorbFormatMP3:
            mimeString = @"mp3";
            break;
        case GlkSoundBlorbFormatMod:
            mimeString = @"mod";
            break;
        case GlkSoundBlorbFormatMIDI:
            mimeString = @"midi";
            break;

        default:
            NSLog(@"schannel_play_ext: unknown resource type (%ld).", format);
            return nil;
    }

    return [NSString stringWithFormat:@"audio/%@", mimeString];
}

#pragma mark Glk request calls from GlkController

- (BOOL)handleFindSoundNumber:(glsi32)resno {
    BOOL result = [self soundIsLoaded:resno];

    if (!result) {
        [_resources[@(resno)] load];
        result = [self soundIsLoaded:resno];
    }
    if (!result)
        _lastsoundresno = -1;
    else
        _lastsoundresno = resno;

    return result;
}

- (void)handleLoadSoundNumber:(glsi32)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       length:(NSUInteger)length {

    if (_lastsoundresno == resno && [self soundIsLoaded:resno])
        return;

    _lastsoundresno = resno;

    [self setSoundID:resno filename:path length:length offset:offset];
}

- (void)setSoundID:(NSInteger)snd filename:(nullable NSString *)filename length:(NSUInteger)length offset:(NSUInteger)offset {

    SoundResource *res = _resources[@(snd)];

    if (res == nil) {
        res = [[SoundResource alloc] initWithFilename:filename offset:offset length:length];
        _resources[@(snd)] = res;
    } else if (res.data) {
        return;
    }

    res.length = length;

    if (filename != nil)
    {
        res.soundFile = _files[filename];
        if (!res.soundFile) {
            res.soundFile = [[SoundFile alloc] initWithPath:filename];
            res.soundFile.handler = self;
            _files[filename] = res.soundFile;
        }
    } else return;
    res.offset = offset;
    [res load];
}

- (int)handleNewSoundChannel:(glui32)volume {
    int i;
    for (i = 0; i < MAXSND; i++)
        if (_glkchannels[@(i)] == nil)
            break;

    if (i == MAXSND)
        return -1;

    _glkchannels[@(i)] = [[GlkSoundChannel alloc] initWithHandler:self
                                                             name:i
                                                           volume:volume];
    return i;
}

- (void)handleDeleteChannel:(int)channel {
    if (_glkchannels[@(channel)]) {
        _glkchannels[@(channel)] = nil;
    }
}

- (void)handleSetVolume:(glui32)volume channel:(int)channel duration:(glui32)duration notify:(glui32)notify {
    GlkSoundChannel *glkchan = _glkchannels[@(channel)];
    if (glkchan) {
        [glkchan setVolume:volume duration:duration notification:notify];
    }
}

- (void)handlePlaySoundOnChannel:(int)channel repeats:(glsi32)repeats notify:(glui32)notify {
    if (_lastsoundresno != -1) {
        GlkSoundChannel *glkchan = _glkchannels[@(channel)];
        if (glkchan) {
            if (_resources[@(_lastsoundresno)].type == GlkSoundBlorbFormatMIDI) {
                if (![glkchan isKindOfClass:[MIDIChannel class]]) {
                    _glkchannels[@(channel)] = [[MIDIChannel alloc] initWithHandler:self
                                                                               name:channel volume:GLK_MAXVOLUME];
                    [glkchan copyValues:_glkchannels[@(channel)]];
                    glkchan = _glkchannels[@(channel)];
                }
            } else if ([glkchan isKindOfClass:[MIDIChannel class]]) {
                _glkchannels[@(channel)] = [[GlkSoundChannel alloc] initWithHandler:self
                                                                               name:channel volume:GLK_MAXVOLUME];
                [glkchan copyValues:_glkchannels[@(channel)]];
                glkchan = _glkchannels[@(channel)];
            }

            BOOL result = [glkchan playSound:_lastsoundresno countOfRepeats:repeats notification:notify];
            if (!result) {
                NSLog(@"handlePlaySoundOnChannel: failed to play sound %d on channel %d.", _lastsoundresno, channel);
            }
        }
    }
}

- (void)handleStopSoundOnChannel:(int)channel {
    GlkSoundChannel *glkchan = _glkchannels[@(channel)];
    if (glkchan) {
        [glkchan stop];
    }
}

- (void)handlePauseOnChannel:(int)channel {
    GlkSoundChannel *glkchan = _glkchannels[@(channel)];
    if (glkchan) {
        [glkchan pause];
    }
}

- (void)handleUnpauseOnChannel:(int)channel {
    GlkSoundChannel *glkchan = _glkchannels[@(channel)];
    if (glkchan) {
        [glkchan unpause];
    }
}

- (void)restartAll {
    for (GlkSoundChannel *chan in _glkchannels.allValues) {
        chan.handler = self;
        [chan restartInternal];
    }
}

- (void)stopAllAndCleanUp {
    if (!_glkchannels.count)
        return;
    for (GlkSoundChannel* chan in _glkchannels.allValues) {
        [chan stop];
    }
}

#pragma mark Called from GlkSoundChannel

- (void)handleVolumeNotification:(unsigned int)notify {
    GlkEvent *gev = [[GlkEvent alloc] initVolumeNotify:notify];
    [_glkctl queueEvent:gev];
}

- (void)handleSoundNotification:(glui32)notify withSound:(glsi32)sound {
    GlkEvent *gev = [[GlkEvent alloc] initSoundNotify:notify withSound:sound];
    [_glkctl queueEvent:gev];
}

-(GlkSoundBlorbFormatType)loadSoundResourceFromSound:(glsi32)snd data:(NSData *__autoreleasing*)buf {

    GlkSoundBlorbFormatType type = GlkSoundBlorbFormatNone;
    SoundResource *resource = _resources[@(snd)];

    if (resource)
    {
        if (!resource.data)
        {
            [resource load];
            if (!resource.data)
                return GlkSoundBlorbFormatNone;
        }
        *buf = resource.data;
        type = resource.type;
    }

    return type;
}

@end

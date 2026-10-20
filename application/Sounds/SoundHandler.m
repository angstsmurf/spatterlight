//
//  SoundHandler.m
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import "SoundHandler.h"

#import <AppKit/AppKit.h>

#import "GlkSoundChannel.h"
#import "SpectrumBeeper.h"
#import "NowPlayingCoordinator.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkEvent.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

static const int kMaxSoundChannels = 32;

@interface SoundHandler ()
@property (strong, nonatomic) SpectrumBeeper *beeper;
@end

@implementation SoundHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _resources = [NSMutableDictionary new];
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
        /* A missing or undecodable key leaves an empty store rather than nil:
           messages to a nil dictionary would silently drop every new sound
           and channel for the rest of the session. */
        _files = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSString class], [SoundFile class], nil] forKey:@"files"];
        if (!_files)
            _files = [NSMutableDictionary new];
        for (SoundFile *file in _files.allValues)
            file.handler = self;
        _resources = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [SoundResource class], nil] forKey:@"resources"];
        if (!_resources)
            _resources = [NSMutableDictionary new];
        for (SoundResource *res in _resources.allValues) {
            res.soundFile = _files[res.filename];
        }
        _glkchannels = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [GlkSoundChannel class], [MIDIChannel class], nil] forKey:@"gchannels"];
        if (!_glkchannels)
            _glkchannels = [NSMutableDictionary new];
        _lastsoundresno = [decoder containsValueForKey:@"lastsoundresno"] ? [decoder decodeIntForKey:@"lastsoundresno"] : -1;
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
    return _resources[@(soundId)].data != nil;
}

/* The resource for a sound, read in from its file if it has not been yet, or
   nil if there is no such sound or it cannot be read. */
- (nullable SoundResource *)loadedResourceForSound:(glsi32)soundId {
    SoundResource *resource = _resources[@(soundId)];
    if (resource && !resource.data)
        [resource load];
    return resource.data ? resource : nil;
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
    BOOL result = ([self loadedResourceForSound:resno] != nil);
    _lastsoundresno = result ? resno : -1;
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

- (void)setSoundID:(NSInteger)snd filename:(NSString *)filename length:(NSUInteger)length offset:(NSUInteger)offset {

    SoundResource *res = _resources[@(snd)];

    if (res == nil) {
        res = [[SoundResource alloc] initWithFilename:filename offset:offset length:length];
        _resources[@(snd)] = res;
    } else if (res.data) {
        return;
    }

    res.length = length;
    res.offset = offset;

    res.soundFile = _files[filename];
    if (!res.soundFile) {
        res.soundFile = [[SoundFile alloc] initWithPath:filename];
        res.soundFile.handler = self;
        _files[filename] = res.soundFile;
    }
    [res load];
}

- (int)handleNewSoundChannel:(glui32)volume {
    int i;
    for (i = 0; i < kMaxSoundChannels; i++)
        if (_glkchannels[@(i)] == nil)
            break;

    if (i == kMaxSoundChannels)
        return -1;

    _glkchannels[@(i)] = [[GlkSoundChannel alloc] initWithHandler:self
                                                             name:i
                                                           volume:volume];
    return i;
}

- (void)handleDeleteChannel:(int)channel {
    [_glkchannels[@(channel)] stop];
    _glkchannels[@(channel)] = nil;
}

- (void)handleSetVolume:(glui32)volume channel:(int)channel duration:(glui32)duration notify:(glui32)notify {
    [_glkchannels[@(channel)] setVolume:volume duration:duration notification:notify];
}

- (void)handlePlaySoundOnChannel:(int)channel repeats:(glsi32)repeats notify:(glui32)notify {
    GlkSoundChannel *glkchan = _glkchannels[@(channel)];
    if (_lastsoundresno == -1 || !glkchan)
        return;

    BOOL result = [glkchan playSound:_lastsoundresno countOfRepeats:repeats notification:notify];
    if (!result) {
        NSLog(@"handlePlaySoundOnChannel: failed to play sound %d on channel %d.", _lastsoundresno, channel);
    }
}

- (void)handleStopSoundOnChannel:(int)channel {
    [_glkchannels[@(channel)] stop];
}

- (void)handlePauseOnChannel:(int)channel {
    [_glkchannels[@(channel)] pause];
}

- (void)handleUnpauseOnChannel:(int)channel {
    [_glkchannels[@(channel)] unpause];
}

- (void)restartAll {
    for (GlkSoundChannel *chan in _glkchannels.allValues) {
        chan.handler = self;
        [chan restartInternal];
    }
}

- (void)stopAllAndCleanUp {
    for (GlkSoundChannel* chan in _glkchannels.allValues) {
        [chan stop];
    }
    [_beeper stop];
    [[NowPlayingCoordinator shared] clearForHandler:self];
}

#pragma mark Now Playing

- (NSString *)nowPlayingTitle {
    return _glkctl.game.metadata.title;
}

- (NSData *)nowPlayingArtworkData {
    NSObject *data = _glkctl.game.metadata.cover.data;
    return [data isKindOfClass:[NSData class]] ? (NSData *)data : nil;
}

- (void)nowPlayingStateDidChange {
    [[NowPlayingCoordinator shared] refreshForHandler:self];
}

- (NSArray<GlkSoundChannel *> *)eligibleNowPlayingChannels {
    NSMutableArray<GlkSoundChannel *> *eligible = [NSMutableArray new];
    for (GlkSoundChannel *chan in _glkchannels.allValues) {
        if (chan.claimsNowPlaying)
            [eligible addObject:chan];
    }
    return eligible;
}

- (void)pauseEligibleNowPlayingChannels {
    for (GlkSoundChannel *chan in self.eligibleNowPlayingChannels) {
        if (!chan.isPaused)
            [chan pause];
    }
}

- (void)unpauseEligibleNowPlayingChannels {
    for (GlkSoundChannel *chan in self.eligibleNowPlayingChannels) {
        if (chan.isPaused)
            [chan unpause];
    }
}

- (BOOL)hasPlayingEligibleNowPlayingChannel {
    for (GlkSoundChannel *chan in self.eligibleNowPlayingChannels) {
        if (!chan.isPaused)
            return YES;
    }
    return NO;
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
    SoundResource *resource = [self loadedResourceForSound:snd];
    if (!resource)
        return GlkSoundBlorbFormatNone;
    *buf = resource.data;
    return resource.type;
}

#pragma mark ZX Spectrum BEEP

- (SpectrumBeeper *)beeper {
    if (!_beeper)
        _beeper = [SpectrumBeeper new];
    return _beeper;
}

- (void)playSpectrumBeepFrequency:(int)frequency duration:(int)millisecs {
    [self.beeper playFrequency:frequency duration:millisecs];
}

- (void)primeBeepAudio {
    [self.beeper prime];
}

@end

//
//  AudioPlayer.m
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import "AudioResourceHandler.h"

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#import "GlkSoundChannel.h"
#import "GlkController.h"

#define SDL_CHANNELS 64
#define MAX_SOUND_RESOURCES 500

@implementation SoundResource : NSObject

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _filename = [decoder decodeObjectOfClass:[NSURL class] forKey:@"filename"];
    _length = (size_t)[decoder decodeIntForKey:@"length"];
    _offset = [decoder decodeIntForKey:@"offset"];
    _type = [decoder decodeIntForKey:@"type"];
    _loadedflag = NO;
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInt:_length forKey:@"length"];
    [encoder encodeInt:_offset forKey:@"offset"];
    [encoder encodeInt:_type forKey:@"type"];
    [encoder encodeObject:_filename forKey:@"filename"];
}

@end

@implementation AudioResourceHandler

- (instancetype)init {
    self = [super init];
    if (self) {
        _my_resources = [[NSMutableDictionary alloc] init];
        _sound_channels = [[NSMutableDictionary alloc] init];
        [self initializeSound];
    }
    return self;
}

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    _my_resources = [decoder decodeObjectOfClass:[NSMutableDictionary class] forKey:@"resources"];
    _sound_channels = [[NSMutableDictionary alloc] init];
    _restored_music_channel_id = [decoder decodeIntForKey:@"music_channel"];
    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_my_resources forKey:@"resources"];
    [encoder encodeInt:_music_channel.name forKey:@"music_channel"];
}

- (void)initializeSound
{
    if (SDL_Init(SDL_INIT_AUDIO) == -1)
    {
        NSLog(@"SDL init failed\n");
        NSLog(@"%s", SDL_GetError());
        return;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
    {
        NSLog(@"SDL Mixer init failed\n");
        NSLog(@"%s", Mix_GetError());
        return;
    }

    int channels = Mix_AllocateChannels(SDL_CHANNELS);
    Mix_GroupChannels(0, channels - 1 , FREE);
    Mix_ChannelFinished(NULL);
}

- (NSInteger)detect_sound_format:(void *)buf length:(NSUInteger)len
{
    char str[30];
    if (len > 29)
    {
        strncpy(str, buf, 29);
        str[29]='\0';
    } else {
        strncpy(str, buf, len);
        str[len - 1]='\0';
    }
    /* AIFF */
    if (len > 4 && !memcmp(buf, "FORM", 4))
        return giblorb_ID_FORM;

    /* WAVE */
    if (len > 4 && !memcmp(buf, "WAVE", 4))
        return giblorb_ID_WAVE;

    if (len > 4 && !memcmp(buf, "RIFF", 4))
        return giblorb_ID_WAVE;

    /* midi */
    if (len > 4 && !memcmp(buf, "MThd", 4))
        return giblorb_ID_MIDI;

    /* s3m */
    if (len > 0x30 && !memcmp(buf + 0x2c, "SCRM", 4))
        return giblorb_ID_MOD;

    /* XM */
    if (len > 20 && !memcmp(buf, "Extended Module: ", 17))
        return giblorb_ID_MOD;

    /* MOD */
    if (len > 1084)
    {
        char resname[4];
        memcpy(resname, (buf) + 1080, 4);
        fprintf(stderr, "resname:\"%s\"\n", resname);
        if (!strcmp(resname+1, "CHN") ||        /* 4CHN, 6CHN, 8CHN */
            !strcmp(resname+2, "CN") ||         /* 16CN, 32CN */
            !strcmp(resname, "M.K.") || !strcmp(resname, "M!K!") ||
            !strcmp(resname, "FLT4") || !strcmp(resname, "CD81") ||
            !strcmp(resname, "OKTA") || !strcmp(resname, "    "))
            return giblorb_ID_MOD;
    }

    /* ogg */
    if (len > 4 && !memcmp(buf, "OggS", 4))
        return giblorb_ID_OGG;

    return giblorb_ID_MP3;
}

-(NSInteger)load_sound_resource:(NSInteger)snd length:(NSUInteger *)len data:(char **)buf {
    SoundResource *resource = _my_resources[@(snd)];

    NSInteger type = NONE;

    if (resource)
    {
        if (resource.loadedflag == TRUE)
        {
            *len = resource.length;
            if (resource.data)
                *buf = (char *)resource.data.bytes;
            else
                *buf = NULL;
            type = resource.type;
        }
        else if (resource.filename != nil && resource.length > 0)
        {
            NSFileHandle * fileHandle = [NSFileHandle fileHandleForReadingAtPath:resource.filename.path];

            [fileHandle seekToFileOffset:(unsigned long long)resource.offset];
            resource.data = [fileHandle readDataOfLength:resource.length];
            if (resource.data) {
                resource.loadedflag = YES;
                *len = resource.length;
                *buf = (char *)resource.data.bytes;
                type = resource.type;
            } else {
                NSLog(@"Could not read sound resource from file %@, length %ld, offset %ld\n",resource.filename.path, resource.length, resource.offset);
                return 0;
            }
        }
    }

    [self setSoundResource:(NSInteger)snd type:type data:[NSData dataWithBytes:*buf length:*len] length:*len filename:nil offset:0];

    return type;
}

- (void)setSoundResource:(NSInteger)snd type:(kBlorbSoundFormatType)type data:(NSData *)data length:(NSUInteger)length filename:(nullable NSString *)filename offset:(NSInteger)offset {
    SoundResource *res = _my_resources[@(snd)];

    if (res == nil)
    {
        res = [[SoundResource alloc] init];
        _my_resources[@(snd)] = res;
    }
    else if (res.loadedflag == YES)
    {
        return;
    }

    if (type == NONE && data && length)
        type = [self detect_sound_format:(char *)data.bytes length:length];

    res.type = type;
    res.length = length;
    if (filename != nil)
    {
        res.filename = [NSURL fileURLWithPath:filename relativeToURL:nil];
    }
    else res.filename = nil;

    res.offset = offset;
    if (data)
    {
        res.data = data;
        res.loadedflag = YES;
    }
}

- (BOOL)soundIsLoaded:(NSInteger)soundId {
    SoundResource *resource = _my_resources[@(soundId)];
    if (resource)
        return resource.loadedflag;
    return NO;
}

- (void)stopAllAndCleanUp:(NSArray *)channels {
    if (!channels.count)
        return;
    Mix_ChannelFinished(NULL);
    Mix_HookMusicFinished(NULL);
    for (GlkSoundChannel* chan in channels) {
        [chan stop];
    }
    Mix_CloseAudio();
}

@end

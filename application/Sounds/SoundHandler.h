//
//  SoundHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import <Foundation/Foundation.h>

@class GlkSoundChannel, GlkController;

typedef enum kBlorbSoundFormatType : NSInteger {
    NONE,
    giblorb_ID_MOD,
    giblorb_ID_OGG,
    giblorb_ID_FORM,
    giblorb_ID_AIFF,
    giblorb_ID_MP3,
    giblorb_ID_WAVE,
    giblorb_ID_MIDI,
} kBlorbSoundFormatType;

#define FREE 1
#define BUSY 2

#define MAXSND 32

NS_ASSUME_NONNULL_BEGIN

@interface SoundFile : NSObject

- (instancetype)initWithPath:(NSString *)path;
- (void)resolveBookmark;

@property (nullable) NSData *bookmark;
@property (nullable) NSURL *URL;

@end;


@interface SoundResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;

-(BOOL)load;

@property (nullable) NSData *data;
@property (nullable) SoundFile *soundFile;
@property NSString *filename;
@property NSUInteger offset;
@property NSUInteger length;
@property kBlorbSoundFormatType type;

@end


@interface SoundHandler : NSObject

@property NSMutableDictionary <NSNumber *, SoundResource *> *resources;
@property NSMutableDictionary *sfbplayers;
@property NSMutableDictionary <NSNumber *, GlkSoundChannel *> *glkchannels;
@property NSMutableDictionary <NSString *, SoundFile *> *files;
@property (nullable) GlkSoundChannel *music_channel;
@property NSUInteger restored_music_channel_id;
@property NSInteger lastsoundresno;

@property (weak) GlkController *glkctl;

- (NSInteger)load_sound_resource:(NSInteger)snd length:(NSUInteger *)len data:(char * _Nonnull * _Nonnull)buf;

- (void)restartAll;
- (void)stopAllAndCleanUp;

- (int)handleNewSoundChannel:(int)volume;
- (void)handleDeleteChannel:(int)channel;
- (BOOL)handleFindSoundNumber:(int)resno;
- (void)handleLoadSoundNumber:(int)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       length:(NSUInteger)length;
- (void)handleSetVolume:(int)volume
                channel:(int)channel
               duration:(int)duration
                 notify:(int)notify;
- (void)handlePlaySoundOnChannel:(int)channel repeats:(int)repeats notify:(int)notify;
- (void)handleStopSoundOnChannel:(int)channel;
- (void)handlePauseOnChannel:(int)channel;
- (void)handleUnpauseOnChannel:(int)channel;

- (void)handleVolumeNotification:(NSInteger)notify;
- (void)handleSoundNotification:(NSInteger)notify withSound:(NSInteger)sound;

@end

NS_ASSUME_NONNULL_END

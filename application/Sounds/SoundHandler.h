//
//  SoundHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import <Foundation/Foundation.h>

@class GlkSoundChannel, GlkController, SoundHandler;

typedef uint32_t glui32;
typedef int32_t glsi32;


typedef NS_ENUM(NSInteger, GlkSoundBlorbFormatType) {
	GlkSoundBlorbFormatNone,
    GlkSoundBlorbFormatMod,
	GlkSoundBlorbFormatOggVorbis,
	GlkSoundBlorbFormatFORM,
	GlkSoundBlorbFormatAIFF,
	GlkSoundBlorbFormatMP3,
	GlkSoundBlorbFormatWave,
	GlkSoundBlorbFormatMIDI,
};

#define MAXSND 32

NS_ASSUME_NONNULL_BEGIN

@interface SoundFile : NSObject

- (instancetype)initWithPath:(NSString *)path;
- (void)resolveBookmark;

@property (nullable) NSData *bookmark;
@property (nullable) NSURL *URL;
@property (weak) SoundHandler *handler;

@end


@interface SoundResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL load;

@property (copy, nullable) NSData *data;
@property (strong, nullable) SoundFile *soundFile;
@property NSString *filename;
@property NSUInteger offset;
@property NSUInteger length;
@property GlkSoundBlorbFormatType type;
+ (GlkSoundBlorbFormatType)detectSoundFormatFromData:(NSData*)data;

@end


@interface SoundHandler : NSObject

@property (strong) NSMutableDictionary <NSNumber *, SoundResource *> *resources;
@property (strong) NSMutableDictionary *sfbplayers;
@property (strong) NSMutableDictionary <NSNumber *, GlkSoundChannel *> *glkchannels;
@property (strong) NSMutableDictionary <NSString *, SoundFile *> *files;
@property glsi32 lastsoundresno;

@property (weak) GlkController *glkctl;

- (GlkSoundBlorbFormatType)loadSoundResourceFromSound:(glsi32)snd data:(NSData * _Nullable __autoreleasing * _Nonnull)buf;
+ (nullable NSString*)MIMETypeFromFormatType:(GlkSoundBlorbFormatType)format;

- (void)restartAll;
- (void)stopAllAndCleanUp;

- (int)handleNewSoundChannel:(glui32)volume;
- (void)handleDeleteChannel:(int)channel;
- (BOOL)handleFindSoundNumber:(glsi32)resno;

- (void)handleLoadSoundNumber:(glsi32)resno
                         from:(NSString *)path
                       offset:(NSUInteger)offset
                       length:(NSUInteger)length;

- (void)handleSetVolume:(glui32)volume
                channel:(int)channel
               duration:(glui32)duration
                 notify:(glui32)notify;

- (void)handlePlaySoundOnChannel:(int)channel repeats:(glsi32)repeats notify:(glui32)notify;
- (void)handleStopSoundOnChannel:(int)channel;
- (void)handlePauseOnChannel:(int)channel;
- (void)handleUnpauseOnChannel:(int)channel;
- (void)handleVolumeNotification:(glui32)notify;
- (void)handleSoundNotification:(glui32)notify withSound:(glsi32)sound;

@end

NS_ASSUME_NONNULL_END

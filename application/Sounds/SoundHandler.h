//
//  SoundHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import <Foundation/Foundation.h>

#import "SoundResource.h"

@class GlkSoundChannel, GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface SoundHandler : NSObject

@property (strong) NSMutableDictionary <NSNumber *, SoundResource *> *resources;
@property (strong) NSMutableDictionary <NSNumber *, GlkSoundChannel *> *glkchannels;
@property (strong) NSMutableDictionary <NSString *, SoundFile *> *files;
@property glsi32 lastsoundresno;

@property (weak) GlkController *glkctl;

- (GlkSoundBlorbFormatType)loadSoundResourceFromSound:(glsi32)snd data:(NSData * _Nullable __autoreleasing * _Nonnull)buf;
+ (nullable NSString*)MIMETypeFromFormatType:(GlkSoundBlorbFormatType)format;

- (void)restartAll;
- (void)stopAllAndCleanUp;

- (void)nowPlayingStateDidChange;
/// The game's title and cover image data, for system Now Playing.
@property (readonly, nullable) NSString *nowPlayingTitle;
@property (readonly, nullable) NSData *nowPlayingArtworkData;
/// Channels holding music-like sound (see GlkSoundChannel.claimsNowPlaying).
@property (readonly) NSArray<GlkSoundChannel *> *eligibleNowPlayingChannels;
- (void)pauseEligibleNowPlayingChannels;
- (void)unpauseEligibleNowPlayingChannels;
- (BOOL)hasPlayingEligibleNowPlayingChannel;

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

- (void)playSpectrumBeepFrequency:(int)frequency duration:(int)millisecs;
- (void)primeBeepAudio;

- (void)handlePlaySoundOnChannel:(int)channel repeats:(glsi32)repeats notify:(glui32)notify;
- (void)handleStopSoundOnChannel:(int)channel;
- (void)handlePauseOnChannel:(int)channel;
- (void)handleUnpauseOnChannel:(int)channel;
- (void)handleVolumeNotification:(glui32)notify;
- (void)handleSoundNotification:(glui32)notify withSound:(glsi32)sound;

@end

NS_ASSUME_NONNULL_END

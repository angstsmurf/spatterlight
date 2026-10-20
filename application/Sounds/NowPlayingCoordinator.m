//
//  NowPlayingCoordinator.m
//  Spatterlight
//

#import "NowPlayingCoordinator.h"

#import <AppKit/AppKit.h>

#import "SoundHandler.h"
#import "GlkSoundChannel.h"

@interface MPNowPlayingInfoCenter (NowPlayingCoordinator) <NowPlayingInfoCenter>
@end

@interface NowPlayingCoordinator ()
@property (weak, nullable) SoundHandler *activeHandler;
@property (readonly) id<NowPlayingInfoCenter> infoCenter;
/// The last cover decoded, and the data it was decoded from, so that a
/// pause or a new sound does not decode the same image again.
@property (nullable) NSData *artworkData;
@property (nullable) MPMediaItemArtwork *artwork;
@end

@implementation NowPlayingCoordinator

- (instancetype)initWithInfoCenter:(id<NowPlayingInfoCenter>)infoCenter {
    self = [super init];
    if (self)
        _infoCenter = infoCenter;
    return self;
}

+ (NowPlayingCoordinator *)shared {
    static NowPlayingCoordinator *shared;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[NowPlayingCoordinator alloc] initWithInfoCenter:[MPNowPlayingInfoCenter defaultCenter]];
        [shared setupRemoteCommands];
    });
    return shared;
}

- (void)setupRemoteCommands {
    MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
    NowPlayingCoordinator __weak *weakSelf = self;

    // The pause/unpause calls below notify the handler, which refreshes us,
    // so no explicit refresh is needed here.
    [center.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
#pragma unused(event)
        [weakSelf.activeHandler unpauseEligibleNowPlayingChannels];
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    [center.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
#pragma unused(event)
        [weakSelf.activeHandler pauseEligibleNowPlayingChannels];
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    [center.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
#pragma unused(event)
        SoundHandler *handler = weakSelf.activeHandler;
        if ([handler hasPlayingEligibleNowPlayingChannel])
            [handler pauseEligibleNowPlayingChannels];
        else
            [handler unpauseEligibleNowPlayingChannels];
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    // Pause must not be treated as stop: keep position and session.
    center.stopCommand.enabled = NO;
    center.nextTrackCommand.enabled = NO;
    center.previousTrackCommand.enabled = NO;
    center.seekForwardCommand.enabled = NO;
    center.seekBackwardCommand.enabled = NO;
    center.changePlaybackPositionCommand.enabled = NO;
}

- (void)clearForHandler:(SoundHandler *)handler {
    // Never let an idle handler wipe another game window's session. If the
    // owning handler is gone (activeHandler nilled by ARC), allow the clear.
    if (self.activeHandler != nil && self.activeHandler != handler)
        return;

    self.activeHandler = nil;
    _infoCenter.nowPlayingInfo = nil;
    _infoCenter.playbackState = MPNowPlayingPlaybackStateStopped;
}

- (nullable MPMediaItemArtwork *)artworkForData:(nullable NSData *)data {
    if (!data.length)
        return nil;
    if (data == _artworkData || [data isEqualToData:_artworkData])
        return _artwork;

    NSImage *image = [[NSImage alloc] initWithData:data];
    MPMediaItemArtwork *artwork = nil;
    if (image)
        artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:image.size
                                                  requestHandler:^NSImage *(CGSize requestSize) {
#pragma unused(requestSize)
                                                      return image;
                                                  }];
    _artworkData = data;
    _artwork = artwork;
    return artwork;
}

- (void)refreshForHandler:(SoundHandler *)handler {
    BOOL anyPlaying = NO;
    BOOL anyPaused = NO;
    NSTimeInterval duration = 0;
    BOOL looping = NO;

    for (GlkSoundChannel *chan in handler.eligibleNowPlayingChannels) {
        if (chan.isPaused)
            anyPaused = YES;
        else
            anyPlaying = YES;
        if (chan.nowPlayingLooping)
            looping = YES;
        if (chan.nowPlayingDuration > duration)
            duration = chan.nowPlayingDuration;
    }

    if (!anyPlaying && !anyPaused) {
        [self clearForHandler:handler];
        return;
    }

    self.activeHandler = handler;

    NSMutableDictionary *info = [NSMutableDictionary dictionary];

    NSString *appName = [NSRunningApplication currentApplication].localizedName ?: @"Spatterlight";
    NSString *title = handler.nowPlayingTitle;
    info[MPMediaItemPropertyTitle] = title.length ? title : appName;
    info[MPMediaItemPropertyArtist] = appName;

    MPMediaItemArtwork *artwork = [self artworkForData:handler.nowPlayingArtworkData];
    if (artwork)
        info[MPMediaItemPropertyArtwork] = artwork;

    if (!looping && duration > 0)
        info[MPMediaItemPropertyPlaybackDuration] = @(duration);

    info[MPNowPlayingInfoPropertyPlaybackRate] = anyPlaying ? @1.0 : @0.0;

    _infoCenter.nowPlayingInfo = info;
    _infoCenter.playbackState = anyPlaying ? MPNowPlayingPlaybackStatePlaying
                                          : MPNowPlayingPlaybackStatePaused;
}

@end

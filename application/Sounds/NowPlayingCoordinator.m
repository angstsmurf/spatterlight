//
//  NowPlayingCoordinator.m
//  Spatterlight
//

#import "NowPlayingCoordinator.h"

#import <AppKit/AppKit.h>
#import <MediaPlayer/MediaPlayer.h>

#import "SoundHandler.h"
#import "GlkSoundChannel.h"
#import "GlkController.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

@interface NowPlayingCoordinator ()
@property (weak) SoundHandler *handler;
/// YES while this coordinator last published eligible Now Playing info.
@property BOOL ownsNowPlayingSession;
@end

@implementation NowPlayingCoordinator

/// The coordinator that currently owns system Now Playing (weak).
static __weak NowPlayingCoordinator *sActiveCoordinator;

- (instancetype)initWithSoundHandler:(SoundHandler *)handler {
    if (self = [super init]) {
        _handler = handler;
        [NowPlayingCoordinator setupRemoteCommandsIfNeeded];
    }
    return self;
}

+ (void)setupRemoteCommandsIfNeeded {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];

        [center.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
#pragma unused(event)
            NowPlayingCoordinator *active = sActiveCoordinator;
            [active.handler unpauseEligibleNowPlayingChannels];
            [active refresh];
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        [center.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
#pragma unused(event)
            NowPlayingCoordinator *active = sActiveCoordinator;
            [active.handler pauseEligibleNowPlayingChannels];
            [active refresh];
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        [center.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
#pragma unused(event)
            NowPlayingCoordinator *active = sActiveCoordinator;
            SoundHandler *handler = active.handler;
            if ([handler hasPlayingEligibleNowPlayingChannel])
                [handler pauseEligibleNowPlayingChannels];
            else
                [handler unpauseEligibleNowPlayingChannels];
            [active refresh];
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        // Pause must not be treated as stop: keep position and session.
        center.stopCommand.enabled = NO;
        center.nextTrackCommand.enabled = NO;
        center.previousTrackCommand.enabled = NO;
        center.seekForwardCommand.enabled = NO;
        center.seekBackwardCommand.enabled = NO;
        center.changePlaybackPositionCommand.enabled = NO;
    });
}

- (void)clearNowPlayingInfo {
    MPNowPlayingInfoCenter *infoCenter = [MPNowPlayingInfoCenter defaultCenter];
    infoCenter.nowPlayingInfo = nil;
    infoCenter.playbackState = MPNowPlayingPlaybackStateStopped;
}

- (void)clear {
    // Only clear the system UI if we own the session; otherwise another
    // game window's idle SoundHandler would wipe a paused track.
    if (!_ownsNowPlayingSession && sActiveCoordinator != self)
        return;

    _ownsNowPlayingSession = NO;
    if (sActiveCoordinator == self)
        sActiveCoordinator = nil;
    [self clearNowPlayingInfo];
}

- (void)refresh {
    SoundHandler *handler = _handler;
    if (!handler) {
        [self clear];
        return;
    }

    BOOL anyEligible = NO;
    BOOL anyPlaying = NO;
    BOOL anyPaused = NO;
    NSTimeInterval duration = 0;
    BOOL looping = NO;

    for (GlkSoundChannel *chan in handler.glkchannels.allValues) {
        if (!chan.claimsNowPlaying || chan.status == GlkSoundChannelStatusIdle)
            continue;
        anyEligible = YES;
        if (chan.isPaused)
            anyPaused = YES;
        else
            anyPlaying = YES;
        if (chan.nowPlayingLooping)
            looping = YES;
        if (chan.nowPlayingDuration > duration)
            duration = chan.nowPlayingDuration;
    }

    if (!anyEligible) {
        // Only tear down system Now Playing if *this* handler had claimed it.
        // Other SoundHandlers (other game windows) must not wipe a paused session.
        if (_ownsNowPlayingSession)
            [self clear];
        return;
    }

    _ownsNowPlayingSession = YES;
    sActiveCoordinator = self;

    NSMutableDictionary *info = [NSMutableDictionary dictionary];

    NSString *title = handler.glkctl.game.metadata.title;
    if (title.length)
        info[MPMediaItemPropertyTitle] = title;
    else
        info[MPMediaItemPropertyTitle] = @"Spatterlight";

    info[MPMediaItemPropertyArtist] = @"Spatterlight";

    NSObject *coverObj = handler.glkctl.game.metadata.cover.data;
    if ([coverObj isKindOfClass:[NSData class]]) {
        NSData *coverData = (NSData *)coverObj;
        if (coverData.length) {
            NSImage *image = [[NSImage alloc] initWithData:coverData];
            if (image) {
                CGSize size = image.size;
                MPMediaItemArtwork *artwork =
                    [[MPMediaItemArtwork alloc] initWithBoundsSize:size
                                                    requestHandler:^NSImage *(CGSize requestSize) {
#pragma unused(requestSize)
                                                        return image;
                                                    }];
                info[MPMediaItemPropertyArtwork] = artwork;
            }
        }
    }

    if (!looping && duration > 0)
        info[MPMediaItemPropertyPlaybackDuration] = @(duration);

    info[MPNowPlayingInfoPropertyPlaybackRate] = anyPlaying ? @1.0 : @0.0;

    MPNowPlayingInfoCenter *infoCenter = [MPNowPlayingInfoCenter defaultCenter];
    infoCenter.nowPlayingInfo = info;
    if (anyPlaying)
        infoCenter.playbackState = MPNowPlayingPlaybackStatePlaying;
    else if (anyPaused)
        infoCenter.playbackState = MPNowPlayingPlaybackStatePaused;
    else
        infoCenter.playbackState = MPNowPlayingPlaybackStateStopped;
}

@end

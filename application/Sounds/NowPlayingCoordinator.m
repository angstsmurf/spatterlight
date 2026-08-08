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
/// The SoundHandler that currently owns system Now Playing.
@property (weak, nullable) SoundHandler *activeHandler;
@end

@implementation NowPlayingCoordinator

+ (NowPlayingCoordinator *)shared {
    static NowPlayingCoordinator *shared;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [NowPlayingCoordinator new];
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
    MPNowPlayingInfoCenter *infoCenter = [MPNowPlayingInfoCenter defaultCenter];
    infoCenter.nowPlayingInfo = nil;
    infoCenter.playbackState = MPNowPlayingPlaybackStateStopped;
}

- (void)refreshForHandler:(SoundHandler *)handler {
    BOOL anyPlaying = NO;
    BOOL anyPaused = NO;
    NSTimeInterval duration = 0;
    BOOL looping = NO;

    for (GlkSoundChannel *chan in handler.glkchannels.allValues) {
        if (!chan.claimsNowPlaying)
            continue;
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
    infoCenter.playbackState = anyPlaying ? MPNowPlayingPlaybackStatePlaying
                                          : MPNowPlayingPlaybackStatePaused;
}

@end

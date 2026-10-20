//
//  NowPlayingCoordinator.h
//  Spatterlight
//

#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

@class SoundHandler;

NS_ASSUME_NONNULL_BEGIN

/// The part of MPNowPlayingInfoCenter the coordinator writes to.
@protocol NowPlayingInfoCenter <NSObject>
@property (nullable, copy) NSDictionary<NSString *, id> *nowPlayingInfo;
@property MPNowPlayingPlaybackState playbackState;
@end

/// Publishes system Now Playing (MPNowPlayingInfoCenter) for whichever
/// SoundHandler most recently had music-like sound. One shared instance
/// arbitrates between game windows.
@interface NowPlayingCoordinator : NSObject

+ (NowPlayingCoordinator *)shared;

/// A coordinator that publishes to `infoCenter` and handles no remote
/// commands. +shared is one of these on the system's default center, with
/// the remote commands set up.
- (instancetype)initWithInfoCenter:(id<NowPlayingInfoCenter>)infoCenter NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// The handler that owns the session, if any.
@property (weak, readonly, nullable) SoundHandler *activeHandler;

/// Recompute Now Playing from this handler's channels, taking over or
/// releasing the system session as appropriate.
- (void)refreshForHandler:(SoundHandler *)handler;

/// Clear system Now Playing if this handler owns the session.
- (void)clearForHandler:(SoundHandler *)handler;

@end

NS_ASSUME_NONNULL_END

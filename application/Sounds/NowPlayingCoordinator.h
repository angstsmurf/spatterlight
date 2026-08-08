//
//  NowPlayingCoordinator.h
//  Spatterlight
//

#import <Foundation/Foundation.h>

@class SoundHandler;

NS_ASSUME_NONNULL_BEGIN

/// Publishes system Now Playing (MPNowPlayingInfoCenter) for whichever
/// SoundHandler most recently had music-like sound. One shared instance
/// arbitrates between game windows.
@interface NowPlayingCoordinator : NSObject

+ (NowPlayingCoordinator *)shared;

/// Recompute Now Playing from this handler's channels, taking over or
/// releasing the system session as appropriate.
- (void)refreshForHandler:(SoundHandler *)handler;

/// Clear system Now Playing if this handler owns the session.
- (void)clearForHandler:(SoundHandler *)handler;

@end

NS_ASSUME_NONNULL_END

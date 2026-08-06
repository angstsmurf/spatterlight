//
//  NowPlayingCoordinator.h
//  Spatterlight
//

#import <Foundation/Foundation.h>

@class SoundHandler;

NS_ASSUME_NONNULL_BEGIN

@interface NowPlayingCoordinator : NSObject

- (instancetype)initWithSoundHandler:(SoundHandler *)handler;

/// Recompute Now Playing state from the handler's channels.
- (void)refresh;

/// Clear Now Playing (stopped) and drop metadata.
- (void)clear;

@end

NS_ASSUME_NONNULL_END

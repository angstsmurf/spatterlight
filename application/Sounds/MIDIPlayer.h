//
//  MIDIPlayer.h
//  MIDIPlayer
//
//  Based on code by 谢小进.
//

#import <Foundation/Foundation.h>

@interface MIDIPlayer : NSObject

@property (nonatomic) double progress;
/// Sequence length in seconds (longest track), or 0 if unknown.
@property (nonatomic, readonly) NSTimeInterval duration;

- (instancetype)initWithData:(NSData *)data;

- (void)play;
- (void)stop;
- (void)pause;
- (void)setVolume:(CGFloat)volume;
- (void)loop:(NSInteger)repeats;
- (void)addCallback:(void (^)(void))block;

@end

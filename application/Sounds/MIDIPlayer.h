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
/// Playback position in beats: where the player is now, or where a paused or
/// seeked player will resume.
@property (nonatomic, readonly) double position;

- (instancetype)initWithData:(NSData *)data;

- (void)play;
- (void)stop;
- (void)pause;
- (void)setVolume:(CGFloat)volume;
- (void)loop:(NSInteger)repeats;
/// Resume at `beats` when playback next starts, bringing the synth to the state
/// the sequence would have left it in by that point. A position past the end of a
/// looping sequence is folded back into it.
- (void)seekToPosition:(double)beats;
- (void)addCallback:(void (^)(void))block;

@end

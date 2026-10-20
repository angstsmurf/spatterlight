//
//  MIDIPlayer.h
//  MIDIPlayer
//
//  Based on code by 谢小进.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MIDIPlayer : NSObject

/// Sequence length in seconds (longest track), or 0 if unknown.
@property (nonatomic, readonly) NSTimeInterval duration;
/// Playback position in beats: where the player is now, or where a paused or
/// seeked player will resume.
@property (nonatomic, readonly) double position;

/// nil if the audio graph cannot be set up or the data is not a MIDI file.
- (nullable instancetype)initWithData:(NSData *)data;

/// Returns NO if playback could not be started.
- (BOOL)play;
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

NS_ASSUME_NONNULL_END

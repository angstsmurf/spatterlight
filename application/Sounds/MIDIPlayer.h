//
//  MIDIPlayer.h
//  MIDIPlayer
//
//  Based on code by 谢小进.
//

#import <Foundation/Foundation.h>

@interface MIDIPlayer : NSObject

@property (nonatomic) double progress;

- (instancetype)initWithData:(NSData *)data;

- (void)play;
- (void)stop;
- (void)pause;
- (void)setVolume:(CGFloat)volume;
- (void)loop:(NSInteger)repeats;
- (void)addCallback:(void (^)(void))block;

@end

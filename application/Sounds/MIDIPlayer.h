//
//  MIDIPlayer.h
//  MIDIPlayer
//
//  Created by 谢小进 on 15/4/27.
//  Copyright (c) 2015年 Seedfield. All rights reserved.
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

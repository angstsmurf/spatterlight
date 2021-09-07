//
// Copyright (c) 2020 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#import "AVAudioFormat+SFBFormatTransformation.h"

@implementation AVAudioFormat (SFBFormatTransformation)

- (nullable AVAudioFormat *)nonInterleavedEquivalent
{
	AudioStreamBasicDescription asbd = *(self.streamDescription);
	if(asbd.mFormatID != kAudioFormatLinearPCM || !asbd.mChannelsPerFrame)
		return nil;

	if(asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved)
		return self;

	asbd.mFormatFlags |= kAudioFormatFlagIsNonInterleaved;

	asbd.mBytesPerPacket /= asbd.mChannelsPerFrame;
	asbd.mBytesPerFrame /= asbd.mChannelsPerFrame;

	return [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:self.channelLayout];
}

- (nullable AVAudioFormat *)interleavedEquivalent
{
	AudioStreamBasicDescription asbd = *(self.streamDescription);
	if(asbd.mFormatID != kAudioFormatLinearPCM)
		return nil;

	if(!(asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved))
		return self;

	asbd.mFormatFlags &= ~kAudioFormatFlagIsNonInterleaved;

	asbd.mBytesPerPacket *= asbd.mChannelsPerFrame;
	asbd.mBytesPerFrame *= asbd.mChannelsPerFrame;

	return [[AVAudioFormat alloc] initWithStreamDescription:&asbd channelLayout:self.channelLayout];
}

- (nullable AVAudioFormat *)standardEquivalent
{
	if(self.isStandard)
		return self;
	else if(self.channelLayout)
		return [[AVAudioFormat alloc] initStandardFormatWithSampleRate:self.sampleRate channelLayout:self.channelLayout];
	else
		return [[AVAudioFormat alloc] initStandardFormatWithSampleRate:self.sampleRate channels:self.channelCount];
}

@end

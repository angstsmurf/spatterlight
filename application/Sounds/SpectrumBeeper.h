//
//  SpectrumBeeper.h
//  Spatterlight
//
//  ZX Spectrum BEEP tones for the Quill and TaylorMade interpreters,
//  synthesised as square waves and played back to back with NSSound.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SpectrumBeeper : NSObject

/// Queue a tone, to start when the previous one has finished.
- (void)playFrequency:(int)frequency duration:(int)millisecs;
/// Warm up the audio output once, so the first real tone is not clipped.
- (void)prime;
/// Silence the tone sounding now and every tone queued after it.
- (void)stop;

/// The square wave for one BEEP as a WAV file, or nil if it would be empty.
+ (nullable NSData *)WAVWithFrequency:(int)frequency duration:(int)millisecs;
/// A mono 16-bit PCM WAV file of nSamples silent samples.
+ (NSMutableData *)WAVDataWithSampleCount:(uint32_t)nSamples sampleRate:(uint32_t)sampleRate;

@end

NS_ASSUME_NONNULL_END

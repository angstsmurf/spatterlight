//
//  SFBSoundBackend.h
//  Spatterlight
//
//  Plays sampled sound (AIFF, WAV, Ogg Vorbis, MP3, MOD) through
//  SFBAudioEngine. Positions are in frames, counting every pass of a
//  repeating sound, so the repeat count stays the one the sound was started
//  with.
//

#import "GlkSoundBackend.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBSoundBackend : NSObject <GlkSoundBackend>
@end

NS_ASSUME_NONNULL_END

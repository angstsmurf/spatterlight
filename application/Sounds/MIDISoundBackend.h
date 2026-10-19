//
//  MIDISoundBackend.h
//  Spatterlight
//
//  Plays MIDI through MIDIPlayer. Positions are in beats within the current
//  pass, and the repeat count is the number of passes still to play.
//

#import "GlkSoundBackend.h"

NS_ASSUME_NONNULL_BEGIN

@interface MIDISoundBackend : NSObject <GlkSoundBackend>
@end

NS_ASSUME_NONNULL_END

#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * VoiceOver speech (move tracking and spoken move navigation) and the
 * accessibility rotor support for links and images.
 */

@interface GlkTextBufferWindow (Speech)

- (void)repairRestoredMoveRanges;

// Also declared in GlkTextBufferWindow.h; implemented in this category.
- (void)resetLastSpokenString;
- (NSString *)lastMoveString;
- (void)movesRangesFromV6Menu:(NSArray<NSString *> *)menuStrings;

@end

NS_ASSUME_NONNULL_END

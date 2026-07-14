#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Scrolling: deferred auto-scroll and its pagination caps, scroll position
 * store/restore, and the rolling scrollback trim (inline and background).
 */

@interface GlkTextBufferWindow (Scrolling)

- (void)trimScrollbackIfNeeded:(BOOL)wasAtBottom;
- (void)reallyPerformScroll;
- (void)scrollToBottomAnimated:(BOOL)animate respectCaps:(BOOL)respectCaps;

// Also declared in GlkTextBufferWindow.h; implemented in this category.
- (void)scrollToCharacter:(NSUInteger)character withOffset:(CGFloat)offset animate:(BOOL)animate;
- (void)scrollToTop;
- (void)scrollToBottomAnimated:(BOOL)animate;
- (void)storeScrollOffset;
- (void)restoreScroll:(nullable id)sender;
- (void)forceLayout;
+ (NSUInteger)scrollbackCutPointForString:(NSString *)string
                               targetKeep:(NSUInteger)targetKeep
                                safeLimit:(NSUInteger)safeLimit;
- (NSUInteger)scrollbackLimit;
+ (NSUInteger)clampScrollbackLimit:(NSInteger)value;

@end

NS_ASSUME_NONNULL_END

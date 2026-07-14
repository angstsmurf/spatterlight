#import "GlkTextBufferWindow.h"

NS_ASSUME_NONNULL_BEGIN

/*
 * Hyperlink input requests and click handling, for both inline text links
 * and margin-image links.
 */

@interface GlkTextBufferWindow (Hyperlinks)

- (BOOL)myMouseDown:(NSEvent *)theEvent;

@end

NS_ASSUME_NONNULL_END

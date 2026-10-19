#import <AppKit/AppKit.h>

/*
 * An off-screen text view that receives font panel changes on behalf of
 * the Preferences window and forwards them to it.
 */

@interface DummyTextView : NSTextView <NSWindowDelegate>

- (void)updateTextWithAttributes:(NSDictionary *)attributes;

@property id sender;

@end

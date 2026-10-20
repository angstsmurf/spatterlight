//
//  VoiceOverMenu.h
//  Spatterlight
//
//  A menu that ZMenu can speak: a text menu found in the game's windows
//  (ZMenu itself) or an Infocom V6 menu reported by the interpreter
//  (InfocomV6MenuHandler). GlkController's currentMenu is the one showing.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol VoiceOverMenu <NSObject>

// The selected line as it now stands, or NSNotFound.
- (NSUInteger)findSelectedLine;

// The selected line, optionally preceded by the title and followed by
// "Menu item N of M." and the instructions.
- (NSString *)menuLineStringWithTitle:(BOOL)useTitle index:(BOOL)useIndex total:(BOOL)useTotal instructions:(BOOL)useInstructions;

// How to use the menu, as spoken.
- (NSString *)instructionString;

// Whether the title and instructions have been spoken for this menu.
@property BOOL haveSpokenMenu;

@end

// ".\n<noun> <index + 1>[ of <total>].\n"
NSString *VoiceOverMenuIndexString(NSString *noun, NSUInteger index, NSUInteger total, BOOL useTotal);

// "<intro>, "<title>."\n", or "We are in a menu.\n" if title is empty.
NSString *VoiceOverMenuTitleString(NSString *intro, NSString * _Nullable title);

NS_ASSUME_NONNULL_END

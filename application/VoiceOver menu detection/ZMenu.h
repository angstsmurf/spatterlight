//
//  ZMenu.h
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import <Foundation/Foundation.h>

#import "VoiceOverMenu.h"

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

// Speaks the menu showing in a game: an Infocom V6 menu if the interpreter
// reports one, or else a menu found in the text of the game's windows. For
// the latter, ZMenu is itself the VoiceOverMenu.
@interface ZMenu : NSObject <VoiceOverMenu, VoiceOverHelper>

@property(weak, nullable) GlkController *glkctl;
@property BOOL haveSpokenMenu;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

// Looks for a menu in the game's windows and, if one is found, stores its
// lines, commands and selected line. Returns NO if there is no menu.
- (BOOL)detectMenu;
- (void)speakSelectedLine;

@end

NS_ASSUME_NONNULL_END

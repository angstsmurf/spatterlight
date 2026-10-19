//
//  ZMenu.h
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import <Foundation/Foundation.h>

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface ZMenu : NSObject

@property(weak) GlkController *glkctl;
@property BOOL haveSpokenMenu;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

// Looks for a menu in the game's windows and, if one is found, stores its
// lines, commands and selected line. Returns NO if there is no menu.
- (BOOL)detectMenu;
- (void)speakSelectedLine;

- (NSString *)menuLineStringWithTitle:(BOOL)title index:(BOOL)index total:(BOOL)total instructions:(BOOL)instructions;

@end

NS_ASSUME_NONNULL_END

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

// viewStrings holds the eligible texts from every Glk window,
// one NSString per window
@property NSMutableArray<NSString *> *viewStrings;

// Menu lines stored as NSValue objects holding NSRanges
// (referring to the string in attrStr)
@property NSMutableArray<NSValue *> *lines;

// attrStr is an attributed string containing the text of all menu lines,
// excluding the menu commands. This is the string that the NSRanges
// in the "lines" array above refer to
@property NSAttributedString *attrStr;

@property NSDictionary<NSString *, NSString *> *menuCommands;
@property NSArray<NSString *> *menuKeys;
@property NSUInteger selectedLine;
@property BOOL haveSpokenMenu;
@property BOOL recheckNeeded;
@property BOOL isTads3;
@property NSString *lastSpokenString;
@property NSUInteger lastNumberOfItems;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

@property (NS_NONATOMIC_IOSONLY, getter=isMenu, readonly) BOOL menu;
- (void)speakSelectedLine;
- (void)deferredSpeakSelectedLine:(id)sender;

- (NSString *)menuLineStringWithTitle:(BOOL)title Index:(BOOL)index total:(BOOL)total instructions:(BOOL)instructions;

@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger findSelectedLine;

@end

NS_ASSUME_NONNULL_END

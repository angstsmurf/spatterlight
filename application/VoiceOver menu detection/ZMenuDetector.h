//
//  ZMenuDetector.h
//  Spatterlight
//
//  Finds a menu in the text of a game's windows: the cluster of menu lines,
//  the instructions and the selected line. Knows nothing about speech or
//  about GlkController; ZMenu gathers the window texts and speaks the result.
//

#import <Cocoa/Cocoa.h>

#import "GlkController.h"

NS_ASSUME_NONNULL_BEGIN

// A Glk window as the detector sees it. text is nil for windows without
// text, such as graphics windows.
@interface ZMenuWindowText : NSObject

- (instancetype)initWithText:(nullable NSAttributedString *)text isGrid:(BOOL)isGrid frame:(NSRect)frame backgroundColor:(nullable NSColor *)backgroundColor;

@property (readonly, nullable) NSAttributedString *text;
@property (readonly) BOOL isGrid;
@property (readonly) NSRect frame;
@property (readonly, nullable) NSColor *backgroundColor;

@end

// A menu found by ZMenuDetector.
@interface ZMenuDetection : NSObject

// The text of the window holding the menu (the window's live text storage)
// and its default background colour.
@property (readonly) NSAttributedString *text;
@property (readonly, nullable) NSColor *backgroundColor;
// The ranges in text of the menu lines.
@property (readonly) NSArray<NSValue *> *lines;
// The instructions: key → command, in the order they were found.
@property (readonly) NSDictionary<NSString *, NSString *> *commands;
@property (readonly) NSArray<NSString *> *keys;
@property (readonly) BOOL isTads3;
@property (readonly) BOOL isBeyondZork;
// The selected line when the menu was found.
@property (readonly) NSUInteger selectedLine;

// The selected line as the text now stands, or NSNotFound.
- (NSUInteger)findSelectedLine;

// The text of line index, or @"" if there is no such line.
- (NSString *)stringForLine:(NSUInteger)index;

@end

@interface ZMenuDetector : NSObject

// Looks for a menu in windows, which are all the game's Glk windows.
// Returns nil if there is none.
+ (nullable ZMenuDetection *)detectMenuInWindows:(NSArray<ZMenuWindowText *> *)windows format:(nullable NSString *)format gameID:(kGameIdentity)gameID;

// The menu title: the first line of the topmost window, unless it looks
// like a status line.
+ (NSString *)titleForMenu:(ZMenuDetection *)menu inWindows:(NSArray<ZMenuWindowText *> *)windows;

// The instructions as they are spoken, one "Command: "key"." per line.
+ (NSString *)instructionsForMenu:(ZMenuDetection *)menu;

@end

NS_ASSUME_NONNULL_END

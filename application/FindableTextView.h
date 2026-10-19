//
//  FindableTextView.h
//  Spatterlight
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// An NSTextView that drives its own NSTextFinder, with the enclosing scroll
// view as the find bar container. Shared by the buffer window text view and
// the help panel.
@interface FindableTextView : NSTextView <NSTextFinderClient>

// Created on first use.
@property(readonly) NSTextFinder *textFinder;

// Call after changing the text storage, or search will break.
- (void)resetTextFinder;

// Sends the menu item's tag to the text finder. Validated by
// validateMenuItem:.
- (void)performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender;

@end

NS_ASSUME_NONNULL_END

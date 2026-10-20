//
//  JourneyDialogPresenter.h
//  Spatterlight
//
//  The sheets JourneyMenuHandler asks the game's questions with. Knows
//  nothing about the game: the handler turns the answers into key presses
//  and clicks.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface JourneyDialogPresenter : NSObject

// A text field with an OK button. elvish is YES for any text entry except
// the main character's name, and allows a longer text in other letters.
- (void)presentTextEntryWithText:(NSString *)text elvish:(BOOL)elvish forWindow:(NSWindow *)window completion:(void (^)(NSString *enteredText))completion;

// OK and Cancel.
- (void)presentConfirmationWithText:(NSString *)text forWindow:(NSWindow *)window completion:(void (^)(BOOL confirmed))completion;

// A popup menu of titles with OK and Cancel. chosenTitle is nil on Cancel.
- (void)presentChoiceWithText:(NSString *)text titles:(NSArray<NSString *> *)titles forWindow:(NSWindow *)window completion:(void (^)(NSString * _Nullable chosenTitle))completion;

@end

NS_ASSUME_NONNULL_END

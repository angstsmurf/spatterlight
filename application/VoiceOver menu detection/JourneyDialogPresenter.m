//
//  JourneyDialogPresenter.m
//  Spatterlight
//

#import "JourneyDialogPresenter.h"
#import "JourneyTextFormatter.h"

// The field limits for elvish and for the main character's name.
static const NSUInteger kElvishMaxLength = 13;
static const NSUInteger kNameMaxLength = 8;

@implementation JourneyDialogPresenter

- (NSAlert *)alertWithText:(NSString *)text {
    NSAlert *alert = [NSAlert new];
    alert.messageText = text;
    return alert;
}

// An accessory view holding control, which is 100 × 22 points.
- (NSView *)accessoryViewWithControl:(NSView *)control {
    NSView *accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 100, 32.0)];
    control.frame = NSMakeRect(0.0, 10.0, 100, 22.0);
    [accessoryView addSubview:control];
    return accessoryView;
}

- (void)presentTextEntryWithText:(NSString *)text elvish:(BOOL)elvish forWindow:(NSWindow *)window completion:(void (^)(NSString *enteredText))completion {
    NSAlert *alert = [self alertWithText:text];

    NSTextField *textField = [NSTextField new];
    textField.formatter = [[JourneyTextFormatter alloc] initWithMaxLength:(elvish ? kElvishMaxLength : kNameMaxLength) elvish:elvish];
    alert.accessoryView = [self accessoryViewWithControl:textField];
    alert.window.initialFirstResponder = textField;

    [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse result) {
        completion(textField.stringValue);
    }];
}

- (void)presentConfirmationWithText:(NSString *)text forWindow:(NSWindow *)window completion:(void (^)(BOOL confirmed))completion {
    NSAlert *alert = [self alertWithText:text];
    [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse result) {
        completion(result == NSAlertFirstButtonReturn);
    }];
}

- (void)presentChoiceWithText:(NSString *)text titles:(NSArray<NSString *> *)titles forWindow:(NSWindow *)window completion:(void (^)(NSString * _Nullable chosenTitle))completion {
    NSAlert *alert = [self alertWithText:text];

    NSPopUpButton *popupButton = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [popupButton addItemsWithTitles:titles];
    alert.accessoryView = [self accessoryViewWithControl:popupButton];

    [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    alert.window.initialFirstResponder = popupButton;

    [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse result) {
        completion(result == NSAlertFirstButtonReturn ? popupButton.selectedItem.title : nil);
    }];
}

@end

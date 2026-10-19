//
//  NSAlert+Spatterlight.h
//  Spatterlight
//
//  Building alerts, and the "Do not show this message again" box whose state
//  is kept in the user defaults.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSAlert (Spatterlight)

/// An alert with this message and informative text and these buttons, in
/// order: the first one is the default. With no buttons, AppKit adds an OK
/// button.
+ (instancetype)alertWithMessage:(NSString *)message
                            info:(nullable NSString *)info
                         buttons:(nullable NSArray<NSString *> *)buttons;

/// Shows the suppression check box, with this title or AppKit's default one.
- (void)showSuppressionButtonWithTitle:(nullable NSString *)title;

/// Runs the alert modally. If the suppression box was ticked, whatever the
/// choice, sets suppressionKey in the user defaults.
- (NSModalResponse)runModalSuppressingWithKey:(NSString *)suppressionKey;

/// The same as a sheet on window.
- (void)beginSheetModalForWindow:(NSWindow *)window
              suppressingWithKey:(NSString *)suppressionKey
               completionHandler:(void (^ _Nullable)(NSModalResponse returnCode))handler;

@end

NS_ASSUME_NONNULL_END

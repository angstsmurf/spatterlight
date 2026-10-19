//
//  NSAlert+Spatterlight.m
//  Spatterlight
//

#import "NSAlert+Spatterlight.h"

@implementation NSAlert (Spatterlight)

+ (instancetype)alertWithMessage:(NSString *)message
                            info:(NSString *)info
                         buttons:(NSArray<NSString *> *)buttons {
    NSAlert *alert = [[self alloc] init];
    alert.messageText = message;
    if (info)
        alert.informativeText = info;
    for (NSString *title in buttons)
        [alert addButtonWithTitle:title];
    return alert;
}

- (void)showSuppressionButtonWithTitle:(NSString *)title {
    self.showsSuppressionButton = YES;
    if (title)
        self.suppressionButton.title = title;
}

- (void)rememberSuppressionWithKey:(NSString *)suppressionKey {
    if (self.suppressionButton.state == NSControlStateValueOn) {
        // Suppress this alert from now on
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:suppressionKey];
    }
}

- (NSModalResponse)runModalSuppressingWithKey:(NSString *)suppressionKey {
    NSModalResponse choice = [self runModal];
    [self rememberSuppressionWithKey:suppressionKey];
    return choice;
}

- (void)beginSheetModalForWindow:(NSWindow *)window
              suppressingWithKey:(NSString *)suppressionKey
               completionHandler:(void (^)(NSModalResponse))handler {
    [self beginSheetModalForWindow:window completionHandler:^(NSModalResponse returnCode) {
        [self rememberSuppressionWithKey:suppressionKey];
        if (handler)
            handler(returnCode);
    }];
}

@end

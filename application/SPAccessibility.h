//
//  SPAccessibility.h
//  Spatterlight
//
//  Shared VoiceOver announcement helper.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Ask VoiceOver to speak string, posted from window, or from the main window
// when window is nil. Does nothing for an empty string or when there is no
// window to post from.
static inline void SPAnnounce(NSString * _Nullable string,
                              NSAccessibilityPriorityLevel priority,
                              NSWindow * _Nullable window) {
    if (string.length == 0)
        return;
    if (!window)
        window = NSApp.mainWindow;
    if (!window)
        return;
    NSAccessibilityPostNotificationWithUserInfo(window,
                                                NSAccessibilityAnnouncementRequestedNotification,
                                                @{ NSAccessibilityPriorityKey: @(priority),
                                                   NSAccessibilityAnnouncementKey: string });
}

NS_ASSUME_NONNULL_END

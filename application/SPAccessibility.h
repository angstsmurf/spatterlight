//
//  SPAccessibility.h
//  Spatterlight
//
//  Shared VoiceOver announcement helper.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Ask VoiceOver to speak string, posted from window, or when window is nil
// from the main window, the key window (a panel such as Preferences can be
// key with no main window), or failing those the application itself. Does
// nothing for an empty string.
static inline void SPAnnounce(NSString * _Nullable string,
                              NSAccessibilityPriorityLevel priority,
                              NSWindow * _Nullable window) {
    if (string.length == 0)
        return;
    id element = window ?: NSApp.mainWindow ?: NSApp.keyWindow ?: (id)NSApp;
    if (!element)
        return;
    NSAccessibilityPostNotificationWithUserInfo(element,
                                                NSAccessibilityAnnouncementRequestedNotification,
                                                @{ NSAccessibilityPriorityKey: @(priority),
                                                   NSAccessibilityAnnouncementKey: string });
}

NS_ASSUME_NONNULL_END

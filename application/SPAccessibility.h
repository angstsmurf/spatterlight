//
//  SPAccessibility.h
//  Spatterlight
//
//  Shared VoiceOver announcement helper.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Trace VoiceOver-related calls to stderr when SPATTERLIGHT_VO_AXLOG is set
// in the environment (investigation aid).
static inline BOOL SPVOTraceEnabled(void) {
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("SPATTERLIGHT_VO_AXLOG") != NULL;
    return enabled > 0;
}
#define SPVOTRACE(fmt, ...) \
    do { if (SPVOTraceEnabled()) { \
        fprintf(stderr, "VOAX %.3f %s " fmt "\n", [NSDate date].timeIntervalSince1970, __func__, ##__VA_ARGS__); \
        fflush(stderr); } } while (0)

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
    SPVOTRACE("prio %ld \"%.60s\"", (long)priority, string.UTF8String);
    NSAccessibilityPostNotificationWithUserInfo(element,
                                                NSAccessibilityAnnouncementRequestedNotification,
                                                @{ NSAccessibilityPriorityKey: @(priority),
                                                   NSAccessibilityAnnouncementKey: string });
}

NS_ASSUME_NONNULL_END

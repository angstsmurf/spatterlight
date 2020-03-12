//
//  Compatibility.h
//
//  For compatibility with modern macOS while compiling on 10.7

#ifndef Spatterlight_compatibility_h
#define Spatterlight_compatibility_h

#ifndef NS_OPTIONS
#define NS_OPTIONS(_type, _name)                                               \
enum _name : _type _name;                                                  \
enum _name : _type
#endif

#if !defined(NSAppKitVersion)
typedef double NSAppKitVersion;
#endif

#if !defined(MAC_OS_X_VERSION_10_9) ||                                         \
MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9

typedef NSString *const NSAccessibilityNotificationUserInfoKey;

typedef enum NSAccessibilityPriorityLevel : NSInteger {
    NSAccessibilityPriorityHigh = 90
} NSAccessibilityPriorityLevel;


APPKIT_EXTERN void
NSAccessibilityPostNotificationWithUserInfo(id element, NSString *notification,
                                            NSDictionary *userInfo)
NS_AVAILABLE_MAC(10_7);
APPKIT_EXTERN NSString *const
NSAccessibilityAnnouncementRequestedNotification NS_AVAILABLE_MAC(10_7);
APPKIT_EXTERN NSString *const
NSAccessibilityAnnouncementKey NS_AVAILABLE_MAC(10_7);
APPKIT_EXTERN const
NSAppKitVersion NSAppKitVersionNumber10_8 NS_AVAILABLE_MAC(10_7);
APPKIT_EXTERN const
NSAppKitVersion NSAppKitVersionNumber10_9 NS_AVAILABLE_MAC(10_7);

APPKIT_EXTERN NSAccessibilityNotificationUserInfoKey NSAccessibilityPriorityKey
NS_AVAILABLE_MAC(10_7);

#endif // MAC_OS_X_VERSION_10_9

#if !defined(MAC_OS_X_VERSION_10_12) ||                                         \
MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12

APPKIT_EXTERN const
NSAppKitVersion NSAppKitVersionNumber10_12 NS_AVAILABLE_MAC(10_7);

#endif // MAC_OS_X_VERSION_10_12

#endif // Spatterlight_compatibility_h

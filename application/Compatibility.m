//
//  Compatibility.m
//
//  For compatibility with modern macOS while compiling on 10.7

#import "Compatibility.h"

#if !defined(MAC_OS_X_VERSION_10_9) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9

const NSAccessibilityNotificationUserInfoKey NSAccessibilityPriorityKey = @"AXPriorityKey";
const NSAppKitVersion NSAppKitVersionNumber10_8 = 1187.0;
const NSAppKitVersion NSAppKitVersionNumber10_9 = 1265.0;
const NSAppKitVersion NSAppKitVersionNumber10_12 = 1504.0;

#endif  // MAC_OS_X_VERSION_10_9

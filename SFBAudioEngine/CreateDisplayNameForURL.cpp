/*
 * Copyright (c) 2010 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#if !TARGET_OS_IPHONE
# include <ApplicationServices/ApplicationServices.h>
#endif
#include <CoreFoundation/CoreFoundation.h>

#include "CFWrapper.h"
#include "CreateDisplayNameForURL.h"

CFStringRef
SFB::CreateDisplayNameForURL(CFURLRef url)
{
    if (nullptr == url)
        return CFStringRef("");

	assert(nullptr != url);

	CFStringRef displayName = nullptr;

#if !TARGET_OS_IPHONE
	SFB::CFString scheme(CFURLCopyScheme(url));
	if(scheme) {
		bool isFileURL = (kCFCompareEqualTo == CFStringCompare(CFSTR("file"), scheme, kCFCompareCaseInsensitive));

		if(isFileURL) {
			Boolean result = CFURLCopyResourcePropertyForKey(url, kCFURLLocalizedNameKey, &displayName, nullptr);

			if(!result) {
				fprintf(stderr, "CFURLCopyResourcePropertyForKey(kCFURLLocalizedNameKey) failed: %d\n", result);
				displayName = CFURLCopyLastPathComponent(url);
			}
		}
		else {
			displayName = CFURLGetString(url);
			CFRetain(displayName);
		}
	}
	// If scheme is nullptr the URL is probably invalid, but can still be logged
	else {
		displayName = CFURLGetString(url);
		CFRetain(displayName);
	}
#else
	displayName = CFURLGetString(url);
	CFRetain(displayName);
#endif

	return displayName;
}

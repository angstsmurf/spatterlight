#import <Cocoa/Cocoa.h>

#include "glk.h"
#include "protocol.h"

#import "Preferences.h"

#import "GlkEvent.h"
#import "GlkSoundChannel.h"
#import "GlkStyle.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "GlkController.h"

#import "InfoController.h"
#import "LibController.h"
#import "AppDelegate.h"

extern NSArray *gGameFileTypes;
extern NSDictionary *gExtMap;
extern NSDictionary *gFormatMap;

NSString *stringWithLatin1String(const char *cString, int len);
const char *latin1String(NSString *self);

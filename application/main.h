#import <Cocoa/Cocoa.h>

#include "glk.h"
#include "protocol.h"

#import "Preferences.h"

#import "GlkEvent.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "GlkController.h"

#import "LibController.h"
#import "AppDelegate.h"

extern NSArray *gGameFileTypes;
extern NSArray *gGridStyleNames;
extern NSArray *gBufferStyleNames;

extern NSDictionary *gExtMap;
extern NSDictionary *gFormatMap;

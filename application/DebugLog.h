//
//  DebugLog.h
//  Spatterlight
//
//  Import in any .m file that logs with NSLog. Release builds compile the
//  calls out; debug builds log as usual.
//
//  Foundation is imported first so that its own declaration of NSLog is
//  parsed before the macro exists.
//

#import <Foundation/Foundation.h>

#ifndef DEBUG
#define NSLog(...)
#endif

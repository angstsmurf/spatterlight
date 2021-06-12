//
//  NSBitmapImageRep+retro.h
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSBitmapImageRep (retro)

+ (nullable NSBitmapImageRep *)repFromURL:(NSURL *)url;
+ (nullable NSBitmapImageRep *)repFromNeoURL:(NSURL *)url;
+ (nullable NSBitmapImageRep *)repFromMG1URL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END

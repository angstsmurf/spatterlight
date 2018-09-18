//
//  Game+CoreDataClass.h
//  Spatterlight
//
//  Created by Administrator on 2017-06-16.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

#import "InfoController.h"

@class Interpreter, Metadata, NSObject, Settings;

NS_ASSUME_NONNULL_BEGIN

@interface Game : NSManagedObject <NSWindowDelegate>

- (NSURL *) urlForBookmark;
- (void) bookmarkForPath: (NSString *)path;
- (void) showInfoWindow;

@end

NS_ASSUME_NONNULL_END

#import "Game+CoreDataProperties.h"

//
//  Game.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

#import "InfoController.h"

@class Interpreter, Metadata, Settings;

@interface Game : NSManagedObject

@property (nonatomic, retain) NSDate * added;
@property (nonatomic, retain) id fileLocation;
@property (nonatomic, retain) NSNumber * found;
@property (nonatomic, retain) NSString * group;
@property (nonatomic, retain) NSDate * lastModified;
@property (nonatomic, retain) NSDate * lastPlayed;
@property (nonatomic, retain) Interpreter *interpreter;
@property (nonatomic, retain) Metadata *metadata;
@property (nonatomic, retain) Settings *setting;
@property (nonatomic, retain) Settings *override;


@property (readonly, copy) NSURL *urlForBookmark;
- (void) bookmarkForPath: (NSString *)path;

@end

//@interface UrlToBookmarkTransformer : NSValueTransformer
//@end

//
//  Game.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata, Theme;

@interface Game : NSManagedObject

@property (nonatomic, retain) NSDate * added;
@property (nonatomic, retain) id fileLocation;
@property (nonatomic, retain) NSNumber * found;
@property (nonatomic, retain) NSString * ifid;
@property (nonatomic, retain) NSString * group;
@property (nonatomic, retain) NSDate * lastPlayed;
@property (nonatomic, retain) NSString * hashTag;
@property (nonatomic, retain) NSString * version;
@property (nonatomic, retain) Metadata *metadata;
@property (nonatomic, retain) Theme *override;
@property (nonatomic, retain) Theme *setting;

@property (readonly, copy) NSURL *urlForBookmark;
- (void) bookmarkForPath: (NSString *)path;

@end

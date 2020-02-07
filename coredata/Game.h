//
//  Game.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-31.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata, Theme;

@interface Game : NSManagedObject

@property (nonatomic) NSDate *added;
@property (nonatomic) BOOL autosaved;
@property (nonatomic, retain) id fileLocation;
@property (nonatomic) BOOL found;
@property (nonatomic, retain) NSString * group;
@property (nonatomic, retain) NSString * hashTag;
@property (nonatomic, retain) NSString * ifid;
@property (nonatomic) NSDate *lastPlayed;
@property (nonatomic, retain) NSString * version;
@property (nonatomic, retain) Metadata *metadata;
@property (nonatomic, retain) Theme *override;
@property (nonatomic, retain) Theme *theme;

@property (readonly, copy) NSURL *urlForBookmark;
- (void) bookmarkForPath: (NSString *)path;

@end

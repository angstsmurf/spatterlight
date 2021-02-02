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

NS_ASSUME_NONNULL_BEGIN

@interface Game : NSManagedObject

@property (nullable, nonatomic, retain) NSDate *added;
@property (nonatomic) BOOL autosaved;
@property (nullable, nonatomic, copy) NSString *checksum;
@property (nullable, nonatomic, copy) NSString *compiler;
@property (nullable, nonatomic, copy) NSString *detectedFormat;
@property (nullable, nonatomic, retain) NSObject *fileLocation;
@property (nullable, nonatomic, copy) NSString *fileName;
@property (nonatomic) BOOL found;
@property (nullable, nonatomic, copy) NSString *group;
@property (nullable, nonatomic, copy) NSString *hashTag;
@property (nullable, nonatomic, copy) NSString *ifid;
@property (nullable, nonatomic, retain) NSDate *lastPlayed;
@property (nullable, nonatomic, copy) NSString *path;
@property (nullable, nonatomic, copy) NSString *releaseString;
@property (nullable, nonatomic, copy) NSString *serial;
@property (nullable, nonatomic, copy) NSString *version;
@property (nullable, nonatomic, retain) Metadata *metadata;
@property (nullable, nonatomic, retain) Theme *override;
@property (nullable, nonatomic, retain) Theme *theme;

@property (readonly, copy, nullable) NSURL *urlForBookmark;
- (void) bookmarkForPath: (NSString *)path;

@end

NS_ASSUME_NONNULL_END

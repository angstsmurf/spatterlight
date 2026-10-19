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

+ (NSFetchRequest<Game *> *)fetchRequest NS_SWIFT_NAME(fetchRequest());

@property (nullable, nonatomic, retain) NSDate *added;
@property (nonatomic) BOOL autosaved;
@property (nullable, nonatomic, retain) NSObject *blorbFile;
@property (nonatomic) int32_t checksum;
@property (nullable, nonatomic, copy) NSString *compiler;
@property (nullable, nonatomic, copy) NSString *detectedFormat;
@property (nullable, nonatomic, retain) NSObject *fileLocation;
@property (nonatomic) BOOL found;
@property (nullable, nonatomic, copy) NSString *group;
@property (nonatomic) BOOL hasDownloaded;
@property (nullable, nonatomic, copy) NSString *hashTag;
@property (nonatomic) BOOL hidden;
@property (nullable, nonatomic, copy) NSString *ifid;
@property (nullable, nonatomic, retain) NSDate *lastPlayed;
@property (nonatomic) int32_t like;
@property (nullable, nonatomic, copy) NSString *path;
@property (nonatomic) int32_t releaseNumber;
@property (nullable, nonatomic, copy) NSString *serialString;
@property (nullable, nonatomic, copy) NSString *version;
@property (nullable, nonatomic, retain) Metadata *metadata;
@property (nullable, nonatomic, retain) Theme *theme;

@property (readonly, copy, nullable) NSURL *urlForBookmark;
// The game file's name without directory or extension, used to name
// dragged images.
@property (readonly, copy, nullable) NSString *fileBaseName;
- (void) bookmarkForPath: (NSString *)path;

@end

NS_ASSUME_NONNULL_END

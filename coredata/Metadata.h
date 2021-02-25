//
//  Metadata.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-13.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Game, Ifid, Image, Tag;

NS_ASSUME_NONNULL_BEGIN

@interface Metadata : NSManagedObject

@property (nullable, nonatomic, copy) NSString *author;
@property (nullable, nonatomic, copy) NSString *averageRating;
@property (nullable, nonatomic, copy) NSString *bafn;
@property (nullable, nonatomic, copy) NSString *blurb;
@property (nullable, nonatomic, copy) NSString *coverArtDescription;
@property (nullable, nonatomic, copy) NSNumber *coverArtIndex;
@property (nullable, nonatomic, copy) NSString *coverArtURL;
@property (nullable, nonatomic, copy) NSDate *dateEdited;
@property (nullable, nonatomic, copy) NSString *firstpublished;
@property (nullable, nonatomic, copy) NSDate *firstpublishedDate;
@property (nullable, nonatomic, copy) NSString *forgiveness;
@property (nullable, nonatomic, copy) NSNumber *forgivenessNumeric;
@property (nullable, nonatomic, copy) NSString *format;
@property (nullable, nonatomic, copy) NSString *genre;
@property (nullable, nonatomic, copy) NSString *group;
@property (nullable, nonatomic, copy) NSString *headline;
@property (nullable, nonatomic, copy) NSString *language;
@property (nullable, nonatomic, copy) NSString *languageAsWord;
@property (nullable, nonatomic, copy) NSDate *lastModified;
@property (nullable, nonatomic, copy) NSString *myRating;
@property (nullable, nonatomic, copy) NSString *ratingCountTot;
@property (nullable, nonatomic, copy) NSString *series;
@property (nullable, nonatomic, copy) NSString *seriesnumber;
@property (nullable, nonatomic, copy) NSNumber *source;
@property (nullable, nonatomic, copy) NSString *starRating;
@property (nullable, nonatomic, copy) NSString *title;
@property (nullable, nonatomic, copy) NSString *tuid;
@property (nullable, nonatomic, copy) NSNumber *userEdited;
@property (nullable, nonatomic, retain) Image *cover;
@property (nullable, nonatomic, retain) NSSet<Game *> *games;
@property (nullable, nonatomic, retain) NSSet<Ifid *> *ifids;
@property (nullable, nonatomic, retain) Tag *tag;

- (Ifid *)findOrCreateIfid:(NSString *)ifidstring;

@end

@interface Metadata (CoreDataGeneratedAccessors)

- (void)addGamesObject:(Game *)value;
- (void)removeGamesObject:(Game *)value;
- (void)addGames:(NSSet *)values;
- (void)removeGames:(NSSet *)values;

- (void)addIfidsObject:(Ifid *)value;
- (void)removeIfidsObject:(Ifid *)value;
- (void)addIfids:(NSSet *)values;
- (void)removeIfids:(NSSet *)values;

@end

NS_ASSUME_NONNULL_END

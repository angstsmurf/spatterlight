//
//  Metadata.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Game, Image, Tag;

@interface Metadata : NSManagedObject

@property (nonatomic, retain) NSString * author;
@property (nonatomic, retain) NSString * averageRating;
@property (nonatomic, retain) NSString * bafn;
@property (nonatomic, retain) NSString * blurb;
@property (nonatomic, retain) NSString * coverArtURL;
@property (nonatomic, retain) NSDate * dateEdited;
@property (nonatomic, retain) NSDate * firstpublished;
@property (nonatomic, retain) NSNumber * forgiveness;
@property (nonatomic, retain) NSString * format;
@property (nonatomic, retain) NSString * genre;
@property (nonatomic, retain) NSString * group;
@property (nonatomic, retain) NSString * headline;
@property (nonatomic, retain) NSString * ifid;
@property (nonatomic, retain) NSString * language;
@property (nonatomic, retain) NSString * myRating;
@property (nonatomic, retain) NSString * ratingCountTot;
@property (nonatomic, retain) NSString * series;
@property (nonatomic, retain) NSString * seriesIndex;
@property (nonatomic, retain) NSString * starRating;
@property (nonatomic, retain) NSString * title;
@property (nonatomic, retain) NSString * tuid;
@property (nonatomic, retain) NSNumber * userEdited;
@property (nonatomic, retain) NSString * yearAsString;
@property (nonatomic, retain) Image *cover;
@property (nonatomic, retain) Game *game;
@property (nonatomic, retain) Tag *tag;

@end

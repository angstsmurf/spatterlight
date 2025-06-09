//
//  IFDB.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import <Foundation/Foundation.h>

@class Metadata;

@interface IFDB : NSObject

+ (NSString *)pathFromCoverArtElement:(NSXMLElement *)element;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;
- (void)addInfoToMetadata:(Metadata *)metadata;

@property NSString *tuid;
@property NSString *coverArtURL;
@property NSString *starRating;
@property NSURL *link;
@property NSString *averageRating;
@property NSString *ratingCountAvg;
@property NSString *ratingCountTot;

@end

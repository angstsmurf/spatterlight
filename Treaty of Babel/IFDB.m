//
//  IFDB.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFDB.h"
#import "Metadata.h"

@interface IFDB ()

@end

@implementation IFDB

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    for (NSXMLNode *node in element.children) {
      if ([node.name compare:@"tuid"] == 0) {
        _tuid = node.stringValue;
      } else if ([node.name compare:@"coverart"] == 0) {
          _coverArtURL = [IFDB pathFromCoverArtElement:(NSXMLElement *)node];
      } else if ([node.name compare:@"starRating"] == 0) {
          _starRating = node.stringValue;
      } else if ([node.name compare:@"link"] == 0) {
          _link = [NSURL URLWithString:node.stringValue];
      } else if ([node.name compare:@"averageRating"] == 0) {
          _averageRating = node.stringValue;
      } else if ([node.name compare:@"ratingCountAvg"] == 0) {
          _ratingCountAvg = node.stringValue;
      } else if ([node.name compare:@"ratingCountTot"] == 0) {
          _ratingCountTot = node.stringValue;
      }
    }
  }
  return self;
}

+ (NSString *)pathFromCoverArtElement:(NSXMLElement *)element {
  for (NSXMLNode *node in element.children) {
    if ([node.name compare:@"url"] == 0) {
      return node.stringValue;
    }
  }
  return nil;
}

- (void)addInfoToMetadata:(Metadata *)metadata {
    if (_tuid.length)
        metadata.tuid = _tuid;
    if (_starRating.length)
        metadata.starRating = _starRating;
    if (_coverArtURL.length)
        metadata.coverArtURL = _coverArtURL;
}

@end

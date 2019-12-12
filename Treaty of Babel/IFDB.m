//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFDB.h"
#import "Metadata.h"

@interface IFDB ()

@end

@implementation IFDB

- (instancetype)initWithXMLElement:(NSXMLElement *)element andMetadata:(Metadata *)metadata {
  self = [super init];
  if (self) {
    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"tuid"] == 0) {
        metadata.tuid = node.stringValue;
      } else if ([node.name compare:@"coverart"] == 0) {
          metadata.coverArtURL = [self pathFromCoverArtElement:(NSXMLElement *)node];
      } else if ([node.name compare:@"starRating"] == 0) {
          metadata.starRating = node.stringValue;
//      } else if ([node.name compare:@"link"] == 0) {
//        metadata.link = [NSURL URLWithString:node.stringValue];
//      } else if ([node.name compare:@"averageRating"] == 0) {
//        metadata.averageRating = node.stringValue.doubleValue;
//      } else if ([node.name compare:@"ratingCountAvg"] == 0) {
//        metadata.ratingCountAvg = node.stringValue.integerValue;
//      } else if ([node.name compare:@"ratingCountTot"] == 0) {
//        metadata.ratingCountTot = node.stringValue.integerValue;
      }
    }
  }
  return self;
}

- (NSString *)pathFromCoverArtElement:(NSXMLElement *)element {
  NSEnumerator *enumChildren = [element.children objectEnumerator];
  NSXMLNode *node;
  while ((node = [enumChildren nextObject])) {
    if ([node.name compare:@"url"] == 0) {
      return node.stringValue;
    }
  }
  return nil;
}

@end

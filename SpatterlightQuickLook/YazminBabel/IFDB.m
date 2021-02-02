//
//  IFDB.m
//  Yazmin
//
//  Created by David Schweinsberg on 8/18/19.
//  Copyright © 2019 David Schweinsberg. All rights reserved.
//

#import "IFDB.h"

@interface IFDB ()

- (NSURL *)URLFromCoverArtElement:(NSXMLElement *)element;

@end

@implementation IFDB

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"tuid"] == 0) {
        _tuid = node.stringValue;
      } else if ([node.name compare:@"link"] == 0) {
        _link = [NSURL URLWithString:node.stringValue];
      } else if ([node.name compare:@"coverart"] == 0) {
        _coverArt = [self URLFromCoverArtElement:(NSXMLElement *)node];
      } else if ([node.name compare:@"averageRating"] == 0) {
        _averageRating = node.stringValue.doubleValue;
      } else if ([node.name compare:@"starRating"] == 0) {
        _starRating = node.stringValue.doubleValue;
      } else if ([node.name compare:@"ratingCountAvg"] == 0) {
        _ratingCountAvg = node.stringValue.integerValue;
      } else if ([node.name compare:@"ratingCountTot"] == 0) {
        _ratingCountTot = node.stringValue.integerValue;
      }
    }
  }
  return self;
}

- (NSURL *)URLFromCoverArtElement:(NSXMLElement *)element {
  NSEnumerator *enumChildren = [element.children objectEnumerator];
  NSXMLNode *node;
  while ((node = [enumChildren nextObject])) {
    if ([node.name compare:@"url"] == 0) {
      return [NSURL URLWithString:node.stringValue];
    }
  }
  return nil;
}

@end

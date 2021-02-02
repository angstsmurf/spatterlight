//
//  IFAnnotation.m
//  Yazmin
//
//  Created by David Schweinsberg on 8/20/19.
//  Copyright © 2019 David Schweinsberg. All rights reserved.
//

#import "YazIFAnnotation.h"
#import "IFYazmin.h"

@implementation IFAnnotation

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"yazmin"] == 0) {
        _yazmin = [[IFYazmin alloc] initWithXMLElement:(NSXMLElement *)node];
      }
    }
  }
  return self;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _yazmin = [[IFYazmin alloc] init];
  }
  return self;
}

- (NSString *)xmlString {
  NSMutableString *string = [NSMutableString string];
  [string appendString:@"<annotation>\n"];
  [string appendString:_yazmin.xmlString];
  [string appendString:@"</annotation>\n"];
  return string;
}

@end

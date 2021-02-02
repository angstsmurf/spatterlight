//
//  IFIdentification.m
//  Yazmin
//
//  Created by David Schweinsberg on 25/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "YazIFIdentification.h"

@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    NSMutableArray<NSString *> *ifids = [[NSMutableArray alloc] init];
    _format = @"";

    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"ifid"] == 0)
        [ifids addObject:node.stringValue];
      else if ([node.name compare:@"format"] == 0) {
        _format = node.stringValue;
      } else if ([node.name compare:@"bafn"] == 0)
        _bafn = node.stringValue.intValue;
    }
    _ifids = ifids;
  }
  return self;
}

- (instancetype)initWithIFID:(NSString *)ifid {
  self = [super init];
  if (self) {
    _ifids = [NSArray arrayWithObject:ifid];

    // TODO: Revisit this when we're dealing with more than Z-code
    _format = @"zcode";
  }
  return self;
}

- (NSString *)xmlString {
  NSMutableString *string = [NSMutableString string];
  [string appendString:@"<identification>\n"];
  for (NSString *ifid in _ifids)
    [string appendFormat:@"<ifid>%@</ifid>\n", ifid];
  if (![_format isEqualToString:@""])
    [string appendFormat:@"<format>%@</format>\n", _format];
  if (_bafn)
    [string appendFormat:@"<bafn>%d</bafn>\n", _bafn];
  [string appendString:@"</identification>\n"];
  return string;
}

@end

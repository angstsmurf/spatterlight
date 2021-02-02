//
//  IFColophon.m
//  Yazmin
//
//  Created by David Schweinsberg on 8/16/19.
//  Copyright © 2019 David Schweinsberg. All rights reserved.
//

#import "IFColophon.h"

@implementation IFColophon

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"generator"] == 0) {
        _generator = node.stringValue;
      } else if ([node.name compare:@"generatorversion"] == 0) {
        _generatorVersion = node.stringValue;
      } else if ([node.name compare:@"originated"] == 0) {
        _originated = node.stringValue;
      }
    }
  }
  return self;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    NSDictionary<NSString *, id> *info = NSBundle.mainBundle.infoDictionary;
    _generator = info[@"CFBundleName"];
    _generatorVersion = info[@"CFBundleShortVersionString"];
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateFormat:@"LLL d yyyy"];
    NSDate *date = [dateFormatter dateFromString:@(__DATE__)];
    [dateFormatter setDateFormat:@"yyyy-MM-dd"];
    _originated = [dateFormatter stringFromDate:date];
  }
  return self;
}

- (NSString *)xmlString {
  NSMutableString *string = [NSMutableString string];
  [string appendString:@"<colophon>\n"];
  [string appendFormat:@"<generator>%@</generator>\n", _generator];
  if (_generatorVersion)
    [string appendFormat:@"<generatorversion>%@</generatorversion>\n",
                         _generatorVersion];
  [string appendFormat:@"<originated>%@</originated>\n", _originated];
  [string appendString:@"</colophon>\n"];
  return string;
}

@end

//
//  BlorbResource.m
//  Spatterlight
//
//  Based on code from Yazmin by David Schweinsberg
//  https://github.com/yazmin
//
//  Original code created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "BlorbResource.h"

@implementation BlorbResource

- (instancetype)initWithUsage:(FourCharCode)usage
                       number:(uint32_t)number
                        start:(uint32_t)start {
  self = [super init];
  if (self) {
    _usage = usage;
    _number = number;
    _start = start;
  }
  return self;
}

@end

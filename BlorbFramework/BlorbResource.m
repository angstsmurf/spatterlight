//
//  BlorbResource.m
//  Yazmin
//
//  Created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "BlorbResource.h"

@implementation BlorbResource

- (instancetype)initWithUsage:(unsigned int)usage
                       number:(unsigned int)number
                        start:(unsigned int)start {
  self = [super init];
  if (self) {
    _usage = usage;
    _number = number;
    _start = start;
  }
  return self;
}

@end

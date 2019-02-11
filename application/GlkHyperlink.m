//
//  GlkHyperlink.m
//  Spatterlight
//
//  Created by Administrator on 2018-09-16.
//

#import "GlkHyperlink.h"

@implementation GlkHyperlink

- (instancetype) initWithIndex: (NSUInteger)index andPos: (NSUInteger)pos
{
	self = [super init];
	if (self)
	{
		_index = index;
		_startpos = pos;
	}
	return self;
}
@end

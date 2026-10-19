//
//  JourneyMenuItem.m
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "JourneyMenuItem.h"

@implementation JourneyMenuItem

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithTitle:(NSString *)title column:(NSUInteger)column line:(NSUInteger)line tag:(NSInteger)tag {
    self = [super init];
    if (self) {
        self.title = title;
        self.column = column;
        self.line = line;
        self.tag = tag;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        self.title = [decoder decodeObjectOfClass:[NSString class] forKey:@"title"] ?: @"";
        self.column = (NSUInteger)[decoder decodeIntegerForKey:@"column"];
        self.line = (NSUInteger)[decoder decodeIntegerForKey:@"line"];
        self.tag = [decoder decodeIntegerForKey:@"tag"];
        self.actor = [decoder decodeObjectOfClass:[NSString class] forKey:@"actor"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_title forKey:@"title"];
    [encoder encodeInteger:(NSInteger)_column forKey:@"column"];
    [encoder encodeInteger:(NSInteger)_line forKey:@"line"];
    [encoder encodeInteger:_tag forKey:@"tag"];
    [encoder encodeObject:_actor forKey:@"actor"];
}

@end

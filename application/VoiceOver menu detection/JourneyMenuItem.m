//
//  JourneyMenuItem.m
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "JourneyMenuItem.h"

// The bottom rows of the grid window hold the menus, in five equal columns
// after the name column. Columns run from 0 (party commands) to 4.
static const NSInteger kMenuColumns = 5;
static const NSInteger kMenuRows = 5;
static const NSInteger kClickInset = 2;

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

- (BOOL)getClickPoint:(NSPoint *)point inGridOfSize:(NSSize)gridSize bordered:(BOOL)bordered {
    NSInteger width = (NSInteger)gridSize.width;
    NSInteger height = (NSInteger)gridSize.height;

    NSInteger x = kClickInset;
    if (_column >= 1) {
        NSInteger commandWidth = width / kMenuColumns;
        NSInteger nameWidth = width % kMenuColumns;
        x = nameWidth + commandWidth * (NSInteger)_column + kClickInset;
    }

    NSInteger y = height - kMenuRows + (NSInteger)_line;
    if (bordered)
        y--;

    *point = NSMakePoint((CGFloat)x, (CGFloat)y);
    return x >= 0 && y >= 0 && x < width && y < height;
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

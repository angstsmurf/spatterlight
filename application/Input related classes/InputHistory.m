//
//  InputHistory.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-25.
//

#import "InputHistory.h"

// At most HISTORYLEN - 1 lines, plus the edit line.
#define HISTORYLEN 100

@interface InputHistory () {
    // Oldest first, at most HISTORYLEN - 1 lines.
    NSMutableArray<NSString *> *entries;
    // The line being edited when the player last left the present.
    NSString *editLine;
    // Index into entries; entries.count is the present.
    NSUInteger pos;
}
@end

@implementation InputHistory

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        entries = [[NSMutableArray alloc] init];
        editLine = @"";
    }
    return self;
}

// "history" holds the lines, oldest first, followed by the edit line, and
// "historypos" the position in them. Anything malformed gives an empty
// history.
- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [self init];
    if (self) {
        NSArray *lines = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [NSString class], nil] forKey:@"history"];
        if (![lines isKindOfClass:[NSArray class]] || !lines.count)
            return self;
        for (id line in lines)
            if (![line isKindOfClass:[NSString class]])
                return self;

        NSUInteger count = lines.count - 1;
        NSUInteger start = count >= HISTORYLEN ? count - HISTORYLEN + 1 : 0;
        [entries addObjectsFromArray:[lines subarrayWithRange:NSMakeRange(start, count - start)]];
        editLine = lines.lastObject;
        NSInteger oldpos = [decoder decodeIntegerForKey:@"historypos"] - (NSInteger)start;
        pos = (oldpos < 0 || (NSUInteger)oldpos > entries.count) ? entries.count : (NSUInteger)oldpos;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    NSMutableArray *lines = [entries mutableCopy];
    [lines addObject:editLine];
    [encoder encodeObject:lines forKey:@"history"];
    [encoder encodeInteger:(NSInteger)pos forKey:@"historypos"];
}

- (void)reset {
    pos = entries.count;
}

- (void)saveHistory:(NSString *)line {
    [entries addObject:[line copy]];
    if (entries.count >= HISTORYLEN)
        [entries removeObjectAtIndex:0];
    editLine = @"";
    pos = entries.count;
}

- (NSString *)travelBackward:(NSString *)currentInput {
    if (pos == 0)
        return nil;
    if (pos == entries.count)
        editLine = [currentInput copy] ?: @"";
    pos--;
    return entries[pos];
}

- (NSString *)travelForward {
    if (pos >= entries.count)
        return nil;
    pos++;
    return pos == entries.count ? editLine : entries[pos];
}

- (BOOL)isEmpty {
    return entries.count == 0;
}

- (NSUInteger)count {
    return entries.count;
}

@end

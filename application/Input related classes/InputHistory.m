//
//  InputHistory.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-25.
//

#import "InputHistory.h"

// Ring size of the archived format: HISTORYLEN - 1 lines plus the edit line.
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

// The archive keeps the format of the old ring buffer, so that autosaves
// work in both directions: "history" holds HISTORYLEN slots indexed
// modulo HISTORYLEN, the lines run from "historyfirst" up to (not
// including) "historypresent", and the slot at "historypresent" holds the
// edit line. Older versions could write inconsistent states (historyfirst
// == HISTORYLEN, or a ring that had lost slots), so nothing is trusted:
// lines are collected as far as they exist, and anything else gives an
// empty history.
- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [self init];
    if (self) {
        NSArray *ring = [decoder decodeObjectOfClasses:[NSSet setWithObjects:[NSArray class], [NSString class], nil] forKey:@"history"];
        if (![ring isKindOfClass:[NSArray class]])
            return self;
        for (id line in ring)
            if (![line isKindOfClass:[NSString class]])
                return self;

        NSUInteger count = MIN(ring.count, HISTORYLEN);
        NSUInteger first = (NSUInteger)[decoder decodeIntegerForKey:@"historyfirst"] % HISTORYLEN;
        NSUInteger present = (NSUInteger)[decoder decodeIntegerForKey:@"historypresent"] % HISTORYLEN;
        NSUInteger oldpos = (NSUInteger)[decoder decodeIntegerForKey:@"historypos"] % HISTORYLEN;

        pos = NSNotFound;
        for (NSUInteger i = first, steps = 0; i != present && steps < HISTORYLEN; i = (i + 1) % HISTORYLEN, steps++) {
            if (i >= count)
                continue;
            if (i == oldpos)
                pos = entries.count;
            [entries addObject:ring[i]];
        }
        if (entries.count >= HISTORYLEN)
            [entries removeObjectsInRange:NSMakeRange(0, entries.count - HISTORYLEN + 1)];
        if (pos >= entries.count)
            pos = entries.count;
        if (present < count)
            editLine = ring[present];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    NSMutableArray *ring = [entries mutableCopy];
    [ring addObject:editLine];
    [encoder encodeObject:ring forKey:@"history"];
    [encoder encodeInteger:(NSInteger)pos forKey:@"historypos"];
    [encoder encodeInteger:0 forKey:@"historyfirst"];
    [encoder encodeInteger:(NSInteger)entries.count forKey:@"historypresent"];
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

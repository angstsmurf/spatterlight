//
//  InputHistory.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-25.
//

#import "InputHistory.h"

#define HISTORYLEN 100

@interface InputHistory () <NSSecureCoding> {
    /* For command history */
    NSMutableArray *history;
    NSUInteger historypos, historyfirst, historypresent;
}
@end

@implementation InputHistory

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        history = [[NSMutableArray alloc] init];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        history = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"history"];
        historypos = (NSUInteger)[decoder decodeIntegerForKey:@"historypos"];
        historyfirst = (NSUInteger)[decoder decodeIntegerForKey:@"historyfirst"];
        historypresent = (NSUInteger)[decoder decodeIntegerForKey:@"historypresent"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:history forKey:@"history"];
    [encoder encodeInteger:(NSInteger)historypos forKey:@"historypos"];
    [encoder encodeInteger:(NSInteger)historyfirst forKey:@"historyfirst"];
    [encoder encodeInteger:(NSInteger)historypresent forKey:@"historypresent"];
}

- (void)reset {
    historypos = historypresent;
}

- (void)saveHistory:(NSString *)line {
    if (history.count <= historypresent) {
       [history addObject:line];
    } else {
        history[historypresent] = line;
    }

    historypresent++;
    if (historypresent >= HISTORYLEN)
        historypresent -= HISTORYLEN;

    if (historypresent == historyfirst) {
        historyfirst++;
        if (historyfirst > HISTORYLEN)
            historyfirst -= HISTORYLEN;
    }

    if (history.count > historypresent) {
        [history removeObjectAtIndex:historypresent];
    }
}

- (NSString *)travelBackwardInHistory:(NSString *)currentInput {
    NSString *cx;
    if (historypos == historyfirst)
        return nil;

    if (historypos == historypresent) {
        /* save the edited line */
        if (currentInput && currentInput.length)
            cx = currentInput;
        else
            cx = @"";
        if (history.count <= historypos)
            [history addObject:cx];
        else
            history[historypos] = cx;
    }

    if (historypos == 0)
        historypos = HISTORYLEN;

    historypos--;

    cx = history[historypos];
    [self performSelector:@selector(speakString:) withObject:cx afterDelay:0];
    return cx;
}

- (NSString *)travelForwardInHistory {
    if (historypos == historypresent)
        return nil;

    historypos++;
    if (historypos >= HISTORYLEN)
        historypos -= HISTORYLEN;

    NSString *string = history[historypos];
    [self speakString:string];
    return string;
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0)
        return;

    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey : string
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}

@end

//
//  GlkTextGridWindow+Speech.m
//  Spatterlight
//
//  The VoiceOver speech commands: status line and move-by-move reading.
//

#import "GlkTextGridWindowPrivate.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "NSString+Categories.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"

@implementation GlkTextGridWindow (Speech)

#pragma mark Accessibility

- (void)speakStatus {
    GlkController *glkctl = self.glkctl;
    if (glkctl.zmenu)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.zmenu];
    if (glkctl.form)
        [NSObject cancelPreviousPerformRequestsWithTarget:glkctl.form];
    // A merged side-by-side quote box stores its text interleaved; speak the two
    // columns one after the other instead.
    NSString *status = _quoteboxColumns.count ? self.deinterleavedQuoteboxString : textstorage.string;
    [glkctl speakStringNow:status];
}

- (BOOL)setLastMove {
    BOOL hadNewText = _hasNewText;
    _hasNewText = NO;
    return hadNewText;
}

- (void)repeatLastMove:(id)sender {
    if (!self.moveRanges || !self.moveRanges.count) {
        [self speakStatus];
        return;
    }

    moveRangeIndex = self.moveRanges.count - 1;
    NSString *str = [textstorage.string substringWithRangeValue:self.moveRanges.lastObject];

    if (!str.length && sender != self.glkctl) {
        [self.glkctl speakStringNow:@"No last move to speak"];
        return;
    }

    [self.glkctl speakStringNow:str];
}

- (void)speakPrevious {
    if (!self.moveRanges.count)
        return;
    NSString *prefix = @"";
    if (moveRangeIndex > 0) {
        moveRangeIndex--;
    } else {
        prefix = @"At first move.\n";
        moveRangeIndex = 0;
    }
    NSString *str = [prefix stringByAppendingString:[textstorage.string substringWithRangeValue:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (void)speakNext {
    if (!self.moveRanges.count)
    {
        return;
    }

    NSString *prefix = @"";

    if (moveRangeIndex < self.moveRanges.count - 1) {
        moveRangeIndex++;
    } else {
        prefix = @"At last move.\n";
        moveRangeIndex = self.moveRanges.count - 1;
    }

    NSString *str = [prefix stringByAppendingString:[textstorage.string substringWithRangeValue:self.moveRanges[moveRangeIndex]]];
    [self.glkctl speakStringNow:str];
}

- (NSArray *)links {
    NSMutableArray<NSValue *> __block *links = [[NSMutableArray alloc] init];
    [textstorage
     enumerateAttribute:NSLinkAttributeName
     inRange:NSMakeRange(0, textstorage.length)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
         if (!value) {
             return;
         }
         [links addObject:[NSValue valueWithRange:range]];
     }];
    return links;
}

@end

//
//  GlkTextGridWindow+Speech.m
//  Spatterlight
//
//  The VoiceOver speech commands: status line and move-by-move reading.
//

#import "GlkTextGridWindowPrivate.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "GridTextView.h"
#import "NSString+Categories.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"
#import "NSView+Categories.h"

@implementation GlkTextGridWindow (Speech)

#pragma mark Accessibility

- (void)speakStatus {
    [self cancelPendingMenuAnnouncements];
    [self.glkctl speakStringNow:self.speakableString];
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

- (NSString *)speakableStringForMoveRange:(NSValue *)range {
    return [textstorage.string substringWithRangeValue:range];
}

- (NSArray *)links {
    return [_textview linkRangesInRange:NSMakeRange(0, textstorage.length)];
}

@end

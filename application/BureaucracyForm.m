//
//  BureaucracyForm.m
//  Spatterlight
//
//  Created by Administrator on 2020-12-22.
//

#import "BureaucracyForm.h"
#import "GlkController.h"
#import "GlkTextGridWindow.h"

@implementation BureaucracyForm

- (instancetype)initWithGlkController:(GlkController *)glkctl {
    self = [super init];
    if (self) {
        _glkctl = glkctl;
        _fieldstrings = @[@"Last name:", @"First name:", @"Middle initial:", @"Your sex (M/F):", @"House number:", @"Street name:", @"City:", @"State:", @"Zip:", @"Phone:", @"Last employer but one:", @"Least favourite colour:", @"Name of girl/boy friend:", @"Previous girl/boy friend:"];
    }
    return self;
}

- (BOOL)isForm {
    // If the current game is Bureaucracy and the initial text
    // in the grid window is "SOFTWARE LICENCE APPLICATION"
    // we can be be pretty sure that this is it
    if (_glkctl.bureaucracy) {
        for (GlkTextGridWindow *win in _glkctl.gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                _attrStr = win.textview.textStorage;
                if (_attrStr && _attrStr.length < 4000) {
                    NSString *string = _attrStr.string;
                    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                    if (string.length < 29)
                        return NO;
                    string = [string substringToIndex:28];
                    if ([string isEqualToString:@"SOFTWARE LICENCE APPLICATION"]) {
                        _fields = [self extractFieldRanges];
                        _infoFieldRange = [self findInfoFieldRange];
                        NSUInteger currentField = [self findCurrentField];
                        if (currentField != NSNotFound)
                            _selectedField = currentField;
                        [self addMove];
                        return YES;
                    }
                }
            }
        }
    }
    return NO;
}

- (NSArray *)extractFieldRanges {
    NSMutableArray *fields = [[NSMutableArray alloc] init];
    NSString *string = _attrStr.string;
    for (NSString *field in _fieldstrings) {
        NSRange range = [string rangeOfString:field];
        if (range.location == NSNotFound)
            return @[];
        NSValue *rangeVal = [NSValue valueWithRange:range];
        [fields addObject:rangeVal];
    }
    return fields;
}

- (NSUInteger)findCurrentField {
    GlkTextGridWindow *view = (GlkTextGridWindow *)((NSTextStorage *)_attrStr).delegate;
    NSUInteger inputIndex = [view indexOfPos];
    for (NSUInteger index = 0; index < _fields.count; index++) {
        NSRange range = [self rangeFromIndex:index];
        if (inputIndex > range.location && inputIndex < NSMaxRange(range)) {
            return index;
        }
    }
    return NSNotFound;
}

- (NSRange)findInfoFieldRange {
    NSString *string = _attrStr.string;
    NSUInteger startpos = NSMaxRange([string rangeOfString:@"SOFTWARE LICENCE APPLICATION"]);
    NSUInteger endpos = [string rangeOfString:@"Last name:"].location;
    __block NSRange fieldrange;
    __block NSUInteger counter = 0;

    [_attrStr
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:NSMakeRange(startpos, endpos - startpos)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (counter == 5) {
            fieldrange = range;
            *stop = YES;
            return;
        }
        counter++;
    }];
    return fieldrange;
}

- (NSString *)constructInputString {
    NSRange range;
    range.location = NSMaxRange(((NSValue *)_fields [_lastField]).rangeValue);
    if (_lastField == _fields.count - 1)
        range.length = _attrStr.length - range.location - 1;
    else {
        range.length = ((NSValue *)_fields [_lastField + 1]).rangeValue.location - range.location;
    }
    NSString *inputString = [_attrStr.string substringWithRange:range];
    NSString *infoString = [self constructInfoString];
    if ([infoString isEqualToString:_lastInfoString])
        infoString = @"";
    else
        _lastInfoString = infoString;

    NSString *toSpeak = [NSString stringWithFormat:@"%@: %@", inputString, infoString];
    return toSpeak;
}

- (NSString *)constructInfoString {
    NSString *infoString = [_attrStr.string substringWithRange:_infoFieldRange];
    return infoString;
}

- (NSString *)constructFieldString {
    NSUInteger index =  [self findCurrentField];
    if (index == NSNotFound)
        return @"";
    NSRange range = [self rangeFromIndex:index];
    NSString *string = [NSString stringWithFormat:@" On field %ld of 14: %@", index + 1, [_attrStr.string substringWithRange:range]];
    return string;
}

- (NSRange)rangeFromIndex:(NSUInteger)index {
    NSValue *val = _fields[index];
    NSRange range = val.rangeValue;
    NSRange nextrange;
    if (val == _fields.lastObject)
        nextrange = NSMakeRange(_attrStr.string.length, 0);
    else
        nextrange = ((NSValue *)_fields[index + 1]).rangeValue;
    range.length = nextrange.location - range.location;
    return range;
}

- (void)movedFromField {
    NSUInteger foundField = [self findCurrentField];
    if (foundField != _selectedField && foundField != NSNotFound) {
        _lastField = _selectedField;
        _selectedField = foundField;
        _dontSpeakField = NO;
        _glkctl.shouldCheckForMenu = YES;
        [self addMove];
    }
}

- (void)addMove {
    GlkTextGridWindow *win = (GlkTextGridWindow *)((NSTextStorage *)_attrStr).delegate;
    if (win.moveRanges == nil)
        win.moveRanges = [[NSMutableArray alloc] init];
    NSValue *val = [NSValue valueWithRange:[self rangeFromIndex:_selectedField]];
    NSMutableArray *moves = win.moveRanges;
    NSUInteger index = [moves indexOfObject:val];
    if (index != NSNotFound)
        [moves removeObjectAtIndex:index];
    [moves addObject:val];
}

- (void)deferredSpeakString:(id)sender {

    [self speakString:(NSString *)sender];

}

- (void)speakCurrentField {
    if (_dontSpeakField == YES) {
        _dontSpeakField = NO;
        return;
    }

    [self performSelector:@selector(deferredSpeakCurrentField:) withObject:nil afterDelay:0.1];
}

-(void)deferredSpeakCurrentField:(id)sender {
    _selectedField = [self findCurrentField];
    if (_selectedField == NSNotFound)
        return;

    NSString *selectedFieldString = [self constructInputString];

    selectedFieldString =
    [selectedFieldString stringByAppendingString:
     [self constructFieldString]];

    if (!_haveSpokenForm) {
        selectedFieldString = [@"SOFTWARE LICENCE APPLICATION: " stringByAppendingString:selectedFieldString];
        _haveSpokenForm = YES;
    }

    [self speakString:selectedFieldString];
    [self performSelector:@selector(speakInstructions:) withObject:nil afterDelay:5];
}

- (void)speakInstructions:(id)sender {
    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityLow),
        NSAccessibilityAnnouncementKey : @"You mave review the form by stepping through previous moves."
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}

- (void)speakError {
    GlkTextGridWindow *win = (GlkTextGridWindow *)((NSTextStorage *)_attrStr).delegate;
    [win flushDisplay];
    NSString *errorString = [self constructInfoString];
    errorString = [errorString stringByAppendingString:[self constructFieldString]];
    [self speakString:errorString];
}


- (void)speakString:(NSString *)string {
    if (!string || string.length == 0)
        return;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
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

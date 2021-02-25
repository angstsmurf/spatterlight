//
//  BureaucracyForm.m
//  Spatterlight
//
//  Created by Administrator on 2020-12-22.
//

#import "BureaucracyForm.h"
#import "GlkController.h"
#import "GlkTextGridWindow.h"
#import "Theme.h"
#import "Preferences.h"

@implementation BureaucracyForm

- (instancetype)initWithGlkController:(GlkController *)glkctl {
    self = [super init];
    if (self) {
        _haveSpokenForm = NO;
        _glkctl = glkctl;
    }
    return self;
}

- (BOOL)detectForm {
    NSRange titleRange = [self findTitleRange];
    if (titleRange.location == NSNotFound)
        return NO;

    _titlestring = [_attrStr.string substringWithRange:titleRange];

    _titlestring = [_titlestring stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

    if ([_titlestring isEqualToString:@"SOFTWARE LICENCE APPLICATION"]) {
        _fieldstrings = @[@"Last name:", @"First name:", @"Middle initial:", @"Your sex (M/F):", @"House number:", @"Street name:", @"City:", @"State:", @"Zip:", @"Phone:", @"Last employer but one:", @"Least favourite colour:", @"Name of girl/boy friend:", @"Previous girl/boy friend:"];

    } else if ([_titlestring isEqualToString:@"DEPOSIT SLIP"]) {
        _fieldstrings = @[@"Last name:", @"First name:", @"Middle initial:", @"Amount of deposit: $", @"From illegal activity? (y/n):", @"If yes, which one:"];

    } else if ([_titlestring isEqualToString:@"WITHDRAWAL SLIP"]) {
        _fieldstrings = @[@"Last name:", @"First name:", @"Middle initial:", @"Amount of withdrawal: $", @"For illegal activity? (y/n):", @"If yes, which one:"];

    } else if ([_titlestring isEqualToString:@"ZALAGASAN IMMIGRATION FORM"]) {
        _fieldstrings = @[@"Last name:", @"First name:", @"Middle initial:", @"Home planet:", @"Mother's maiden name:", @"Date of last Chinese meal:", @"Ring size:", @"Age of first-born ancestor:", @"Visa number:", @"Stars in universe:", @"Reason for leaving:"];

    } else {
        return NO;
    }

    return YES;
}

- (BOOL)isForm {
    // If the current game is Bureaucracy and the initial text
    // in the grid window is one of the four form titles, then
    // we can be be pretty sure that this is it
    if (_glkctl.bureaucracy) {
        for (GlkTextGridWindow *win in _glkctl.gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                _attrStr = win.textview.textStorage;
                if (_attrStr && _attrStr.length < 4000) {
                    if ([self detectForm]) {
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
    return [self fieldFromPos:inputIndex];
}

- (NSRange)findInfoFieldRange {
    NSString *string = _attrStr.string;
    NSUInteger startpos = NSMaxRange([string rangeOfString:_titlestring]);
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

- (NSRange)findTitleRange {
    NSString *string = _attrStr.string;
    NSUInteger endpos = [string rangeOfString:@"Last name:"].location;

    __block NSRange fieldrange = NSMakeRange(NSNotFound, 0);
    if (endpos == NSNotFound)
        return fieldrange;
    __block NSUInteger counter = 0;

    [_attrStr
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:NSMakeRange(0, endpos)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (counter == 3) {
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
    range.location = NSMaxRange(_fields[_lastField].rangeValue);
    if (_lastField == _fields.count - 1)
        range.length = _attrStr.length - range.location - 1;
    else {
        range.length = _fields[_lastField + 1].rangeValue.location - range.location;
    }
    NSString *inputString = [[_attrStr.string substringWithRange:range] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *infoString = [self constructInfoString];
    if ([infoString isEqualToString:_lastInfoString] && !_speakingError)
        infoString = @"";
    else
        _lastInfoString = infoString;

    NSString *toSpeak = [NSString stringWithFormat:@"%@: %@", inputString, infoString];
    return toSpeak;
}

- (NSString *)constructInfoString {
    NSString *infoString = [_attrStr.string substringWithRange:_infoFieldRange];
    infoString = [infoString stringByReplacingOccurrencesOfString:@"^" withString:@"circumflex"];
    return infoString;
}

- (NSString *)constructFieldStringWithIndex:(BOOL)useIndex andTotal:(BOOL)useTotal {
    NSUInteger index = [self findCurrentField];
    if (index == NSNotFound)
        return @"";
    NSRange range = [self rangeFromIndex:index];
    NSString *string = [_attrStr.string substringWithRange:range];
    if (useIndex) {
        string = [string stringByAppendingString:[NSString stringWithFormat:@"\nField %ld", index + 1]];
        if (useTotal) {
            string = [string stringByAppendingString:[NSString stringWithFormat:@" of %ld", _fields.count]];
        }
        string = [string stringByAppendingString:@"."];
    }
    return string;
}

- (NSRange)rangeFromIndex:(NSUInteger)index {
    NSValue *val = _fields[index];
    NSRange range = val.rangeValue;
    NSRange nextrange;
    if (val == _fields.lastObject)
        nextrange = NSMakeRange(_attrStr.string.length, 0);
    else
        nextrange = _fields[index + 1].rangeValue;
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

    [self checkIfMoved];

    if (_didNotMove)
        _lastField = _selectedField;

    NSString *selectedFieldString = @"";

    if (!_haveSpokenForm || sender == self.glkctl) {
        selectedFieldString =
        [self constructFieldStringWithIndex:YES andTotal:YES];
        if (!_haveSpokenForm) {
            NSString *titleString = [_titlestring stringByAppendingString:@": "];
            titleString = [titleString stringByAppendingString:[self constructInputString]];
            selectedFieldString = [titleString stringByAppendingString:selectedFieldString];
            _haveSpokenForm = YES;
        }
    } else {
        if (self.glkctl.theme.vOSpeakCommand)
            selectedFieldString = [self constructInputString];

        if (!_didNotMove)
            selectedFieldString =
            [selectedFieldString stringByAppendingString:
             [self constructFieldStringWithIndex:(self.glkctl.theme.vOSpeakMenu >= kVOMenuIndex)  andTotal:(self.glkctl.theme.vOSpeakMenu == kVOMenuTotal)]];
    }

    [self speakString:selectedFieldString];
    if (!_haveSpokenInstructions) {
        [self performSelector:@selector(speakInstructions:) withObject:nil afterDelay:7];
        _haveSpokenInstructions = YES;
    }
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
    _speakingError = YES;
    GlkTextGridWindow *win = (GlkTextGridWindow *)((NSTextStorage *)_attrStr).delegate;
    [win flushDisplay];
    [self performSelector:@selector(deferredSpeakError:) withObject:nil afterDelay:0.1];
}

- (void)deferredSpeakError:(id)sender {
    [self checkIfMoved];
    NSString *errorString = @"";
    if (self.glkctl.theme.vOSpeakCommand && !_didNotMove)
        errorString = [self constructInputString];
    else
        errorString = [self constructInfoString];
    errorString = [errorString stringByAppendingString:
                   [self constructFieldStringWithIndex:(self.glkctl.theme.vOSpeakMenu >= kVOMenuIndex) andTotal:(self.glkctl.theme.vOSpeakMenu == kVOMenuTotal)]];
    [self speakString:errorString];
    _speakingError = NO;
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

- (void)checkIfMoved {
    GlkTextGridWindow *view = (GlkTextGridWindow *)((NSTextStorage *)_attrStr).delegate;
    NSUInteger currentPos = [view indexOfPos];
    _didNotMove = ([self fieldFromPos:(NSUInteger)currentPos] == [self fieldFromPos:_lastCharacterPos] );
    _lastCharacterPos = currentPos;
}

- (NSUInteger)fieldFromPos:(NSUInteger)pos {
    for (NSUInteger index = 0; index < _fields.count; index++) {
        NSRange range = [self rangeFromIndex:index];
        if (pos > range.location && pos < NSMaxRange(range)) {
            return index;
        }
    }
    return NSNotFound;
}

@end

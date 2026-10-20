//
//  BureaucracyForm.m
//  Spatterlight
//
//  Created by Administrator on 2020-12-22.
//

#import "BureaucracyForm.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "Theme.h"

// Grid text longer than this is not searched for a form.
static const NSUInteger kMaxFormTextLength = 10000;
// The form title and the info line are these background colour runs
// before the first field.
static const NSUInteger kTitleBackgroundRun = 3;
static const NSUInteger kInfoBackgroundRun = 5;

static const NSTimeInterval kSpeakDelay = 0.4;
static const NSTimeInterval kInstructionsDelay = 7;

// The field labels of each form, keyed by its title.
static NSDictionary<NSString *, NSArray<NSString *> *> *FormFields(void) {
    static NSDictionary *forms;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        forms = @{
            @"SOFTWARE LICENCE APPLICATION" : @[@"Last name:", @"First name:", @"Middle initial:", @"Your sex (M/F):", @"House number:", @"Street name:", @"City:", @"State:", @"Zip:", @"Phone:", @"Last employer but one:", @"Least favourite colour:", @"Name of girl/boy friend:", @"Previous girl/boy friend:"],
            @"DEPOSIT SLIP" : @[@"Last name:", @"First name:", @"Middle initial:", @"Amount of deposit: $", @"From illegal activity? (y/n):", @"If yes, which one:"],
            @"WITHDRAWAL SLIP" : @[@"Last name:", @"First name:", @"Middle initial:", @"Amount of withdrawal: $", @"For illegal activity? (y/n):", @"If yes, which one:"],
            @"ZALAGASAN IMMIGRATION FORM" : @[@"Last name:", @"First name:", @"Middle initial:", @"Home planet:", @"Mother's maiden name:", @"Date of last Chinese meal:", @"Ring size:", @"Age of first-born ancestor:", @"Visa number:", @"Stars in universe:", @"Reason for leaving:"]
        };
    });
    return forms;
}

@interface BureaucracyForm ()

@property NSAttributedString *attrStr;
@property NSArray<NSValue *> *fields;
@property NSArray<NSString *> *fieldstrings;
@property NSString *titlestring;
@property NSUInteger lastField;
@property NSUInteger lastCharacterPos;
@property NSRange infoFieldRange;
@property NSString *lastInfoString;
@property BOOL speakingError;
@property BOOL didNotMove;

@end

@implementation BureaucracyForm

- (instancetype)initWithGlkController:(GlkController *)glkctl {
    self = [super init];
    if (self) {
        _haveSpokenForm = NO;
        _glkctl = glkctl;
    }
    return self;
}

- (GlkTextGridWindow *)gridWindow {
    return (GlkTextGridWindow *)((NSTextStorage *)_attrStr).delegate;
}

- (BOOL)matchFormTitle {
    NSRange titleRange = [self backgroundRun:kTitleBackgroundRun from:0];
    if (titleRange.location == NSNotFound)
        return NO;

    _titlestring = [_attrStr.string substringWithRange:titleRange];
    _titlestring = [_titlestring stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

    _fieldstrings = FormFields()[_titlestring];
    return (_fieldstrings != nil);
}

- (BOOL)detectForm {
    // If the current game is Bureaucracy and the initial text
    // in the grid window is one of the four form titles, then
    // we can be be pretty sure that this is it
    if (_glkctl.gameID == kGameIsBureaucracy) {
        for (GlkTextGridWindow *win in _glkctl.gwindows.allValues) {
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                _attrStr = win.textview.textStorage;
                if (_attrStr && _attrStr.length < kMaxFormTextLength && [self matchFormTitle]) {
                    _fields = [self extractFieldRanges];
                    if (_fields.count == 0)
                        return NO;
                    _infoFieldRange = [self backgroundRun:kInfoBackgroundRun from:NSMaxRange([_attrStr.string rangeOfString:_titlestring])];
                    NSUInteger currentField = [self findCurrentField];
                    if (currentField != NSNotFound)
                        _selectedField = currentField;
                    return YES;
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
    return [self fieldFromPos:self.gridWindow.indexOfPos];
}

// The range of background colour run number `run` (counted from 0) between
// startpos and the first field, or NSNotFound if there are fewer runs.
- (NSRange)backgroundRun:(NSUInteger)run from:(NSUInteger)startpos {
    NSUInteger endpos = [_attrStr.string rangeOfString:@"Last name:"].location;

    NSRange __block fieldrange = NSMakeRange(NSNotFound, 0);
    if (endpos == NSNotFound || startpos > endpos)
        return fieldrange;
    NSUInteger __block counter = 0;

    [_attrStr
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:NSMakeRange(startpos, endpos - startpos)
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (counter == run) {
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
    if (_infoFieldRange.location == NSNotFound || NSMaxRange(_infoFieldRange) > _attrStr.length)
        return @"";
    NSString *infoString = [_attrStr.string substringWithRange:_infoFieldRange];
    infoString = [infoString stringByReplacingOccurrencesOfString:@"^" withString:@"circumflex"];
    return infoString;
}

- (NSString *)fieldStringWithTitle:(BOOL)useTitle andIndex:(BOOL)useIndex andTotal:(BOOL)useTotal {
    NSString *fieldString = [self fieldStringWithIndex:useIndex andTotal:useTotal];
    if (useTitle) {
        fieldString = [NSString stringWithFormat:@"%@: %@ %@", _titlestring, [self constructInputString], fieldString];
    }
    return fieldString;
}


- (NSString *)fieldStringWithIndex:(BOOL)useIndex andTotal:(BOOL)useTotal {
    NSUInteger index = [self findCurrentField];
    if (index == NSNotFound)
        return @"";
    NSRange range = [self rangeFromIndex:index];
    NSString *string = [_attrStr.string substringWithRange:range];
    if (useIndex) {
        string = [string stringByAppendingString:[NSString stringWithFormat:@"\nField %lu", index + 1]];
        if (useTotal) {
            string = [string stringByAppendingString:[NSString stringWithFormat:@" of %lu", _fields.count]];
        }
        string = [string stringByAppendingString:@"."];
    }
    return string;
}

- (NSRange)rangeFromIndex:(NSUInteger)index {
    NSRange range = _fields[index].rangeValue;
    NSRange nextrange;
    if (index == _fields.count - 1)
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
        [self addCurrentFieldToMoves];
    }
}

- (void)addCurrentFieldToMoves {
    GlkTextGridWindow *win = self.gridWindow;
    if (win.moveRanges == nil)
        win.moveRanges = [[NSMutableArray alloc] init];
    NSValue *val = [NSValue valueWithRange:[self rangeFromIndex:_selectedField]];
    NSMutableArray *moves = win.moveRanges;
    NSUInteger index = [moves indexOfObject:val];
    if (index != NSNotFound)
        [moves removeObjectAtIndex:index];
    [moves addObject:val];
}

- (void)cancelPendingSpeech {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)speakCurrentField {
    if (_dontSpeakField == YES) {
        _dontSpeakField = NO;
        return;
    }

    [self performSelector:@selector(deferredSpeakCurrentField:) withObject:nil afterDelay:kSpeakDelay];
}

-(void)deferredSpeakCurrentField:(id)sender {
    _selectedField = [self findCurrentField];
    if (_selectedField == NSNotFound)
        return;

    [self checkIfMoved];

    if (_didNotMove)
        _lastField = _selectedField;

    NSString *selectedFieldString = @"";
    Theme *theme = self.glkctl.theme;

    if (!_haveSpokenForm || sender == self.glkctl) {
        selectedFieldString = [self fieldStringWithTitle:!_haveSpokenForm andIndex:YES andTotal:YES];
        _haveSpokenForm = YES;
    } else {
        if (theme.vOSpeakCommand)
            selectedFieldString = [self constructInputString];

        if (!_didNotMove)
            selectedFieldString =
            [selectedFieldString stringByAppendingString:
             [self fieldStringWithIndex:(theme.vOSpeakMenu >= kVOMenuIndex) andTotal:(theme.vOSpeakMenu == kVOMenuTotal)]];
    }

    [self cancelPendingSpeech];
    [_glkctl speakStringNow:selectedFieldString];
    // Rescheduled after every field, so it comes once the player has
    // stayed in a field for a while.
    if (!_haveSpokenInstructions)
        [self performSelector:@selector(speakInstructions:) withObject:nil afterDelay:kInstructionsDelay];
}

- (void)speakInstructions:(id)sender {
    _haveSpokenInstructions = YES;
    [_glkctl announce:@"You may review the form by stepping through previous moves." priority:NSAccessibilityPriorityLow];
}

- (void)speakError {
    _speakingError = YES;
    [self.gridWindow flushDisplay];
    [self performSelector:@selector(deferredSpeakError:) withObject:nil afterDelay:kSpeakDelay];
}

- (void)deferredSpeakError:(id)sender {
    [self checkIfMoved];
    NSString *errorString;
    if (self.glkctl.theme.vOSpeakCommand && !_didNotMove)
        errorString = [self constructInputString];
    else
        errorString = [self constructInfoString];
    errorString = [errorString stringByAppendingString:
                   [self fieldStringWithIndex:(self.glkctl.theme.vOSpeakMenu >= kVOMenuIndex) andTotal:(self.glkctl.theme.vOSpeakMenu == kVOMenuTotal)]];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(deferredSpeakCurrentField:) object:nil];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(deferredSpeakError:) object:nil];
    [_glkctl speakStringNow:errorString];
    _speakingError = NO;
}

- (void)checkIfMoved {
    NSUInteger currentPos = self.gridWindow.indexOfPos;
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

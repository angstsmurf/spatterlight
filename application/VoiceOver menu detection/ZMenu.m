//
//  ZMenu.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import "ZMenu.h"

#import "GlkController.h"
#import "GlkController+Speech.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "Game.h"
#import "Theme.h"
#import "InfocomV6MenuHandler.h"

#import "NSColor+integer.h"
#import "NSString+Categories.h"
#import "DebugLog.h"

// Larger windows are not searched for menus.
static const NSUInteger kMaxGridTextLength = 10000;
static const NSUInteger kMaxBufferTextLength = 4000;
// A cluster of more lines than this is not a menu.
static const NSUInteger kMaxMenuLines = 25;

static const NSTimeInterval kFirstSpeakDelay = 1.5;
static const NSTimeInterval kInstructionsDelay = 5;

// The text view of a grid or buffer window, nil for any other window.
static NSTextView *TextViewOf(GlkWindow *win) {
    if ([win isKindOfClass:[GlkTextGridWindow class]])
        return ((GlkTextGridWindow *)win).textview;
    if ([win isKindOfClass:[GlkTextBufferWindow class]])
        return ((GlkTextBufferWindow *)win).textview;
    return nil;
}

// Selection marker at the start of a menu line: ">", "*" or "•".
static NSRegularExpression *SelectionMarkerRegex(void) {
    static NSRegularExpression *regex;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        regex = [NSRegularExpression regularExpressionWithPattern:@"^\\s*[>\\*•](?: |)" options:0 error:nil];
    });
    return regex;
}

// The graphic selection indicator in Beyond Zork menus.
static NSRegularExpression *BeyondZorkIndicatorRegex(void) {
    static NSRegularExpression *regex;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        regex = [NSRegularExpression regularExpressionWithPattern:@"X[O-W]{13}Y" options:0 error:nil];
    });
    return regex;
}

// A bracketed key name followed by white space, such as "Enter] ".
static NSRegularExpression *BracketedWordRegex(void) {
    static NSRegularExpression *regex;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        regex = [NSRegularExpression regularExpressionWithPattern:@"\\w+]\\s" options:0 error:nil];
    });
    return regex;
}

// The number of leading spaces (and, if countGreaterThan, '>' characters) in
// range, never counting its last character.
static NSInteger LeadingSpaces(NSString *string, NSRange range, BOOL countGreaterThan) {
    range = NSIntersectionRange(NSMakeRange(0, string.length), range);
    if (range.length == 0)
        return 0;
    NSUInteger spaces = 0;
    unichar c = [string characterAtIndex:range.location];
    while ((c == ' ' || c == 0xa0 || (countGreaterThan && c == '>')) && NSLocationInRange(range.location + spaces + 1, range)) {
        spaces++;
        c = [string characterAtIndex:range.location + spaces];
    }
    return (NSInteger)spaces;
}

// The index of the only color in colors that occurs exactly once, or
// NSNotFound if none or several do.
static NSUInteger UniqueColorIndex(NSArray *colors) {
    NSCountedSet *colorsSet = [[NSCountedSet alloc] initWithArray:colors];
    NSUInteger guess = NSNotFound;
    for (id color in colorsSet) {
        if ([colorsSet countForObject:color] == 1) {
            if (guess != NSNotFound)
                return NSNotFound;
            guess = [colors indexOfObject:color];
        }
    }
    return guess;
}

@interface ZMenu ()

// viewStrings holds the eligible texts from every Glk window,
// one NSString per window
@property NSMutableArray<NSString *> *viewStrings;

// Menu lines stored as NSValue objects holding NSRanges
// (referring to the string in attrStr)
@property NSMutableArray<NSValue *> *lines;

// attrStr is an attributed string containing the text of all menu lines,
// excluding the menu commands. This is the string that the NSRanges
// in the "lines" array above refer to
@property NSAttributedString *attrStr;

@property NSDictionary<NSString *, NSString *> *menuCommands;
@property NSArray<NSString *> *menuKeys;
@property NSUInteger selectedLine;
@property BOOL recheckNeeded;
@property BOOL isTads3;
@property NSString *lastSpokenString;
@property NSUInteger lastNumberOfItems;

@end

@implementation ZMenu

- (instancetype)initWithGlkController:(GlkController *)glkctl {
    self = [super init];
    if (self) {
        _glkctl = glkctl;
    }
    return self;
}

- (BOOL)detectMenu {
    GlkController *glkctl = _glkctl;
    if (glkctl.gameID == kGameIsJuniorArithmancer) {
        return NO;
    }

    NSString *format = glkctl.game.detectedFormat;
    unichar initialChar;

    _isTads3 = [format isEqualToString:@"tads3"];

    if (_isTads3) {
        // If Tads 3: look for a cluster of lines where all start with a '>'.
        // '•' is only used in Thaumistry: Charm's Way.
        if (glkctl.gameID == kGameIsThaumistry)
            initialChar = u'•';
        else
            initialChar = '>';
    } else if ([format isEqualToString:@"glulx"] || [format isEqualToString:@"zcode"] || [format isEqualToString:@"hugo"]) {
        if (glkctl.showingInfocomV6Menu) {
            return YES;
        }
        initialChar = ' ';
    } else {
        return NO; // We only support menus in Glulx or Zcode or Hugo or Tads 3
    }

    _lines = [[self findLargestClusterStartingWithCharacter:initialChar] mutableCopy];

    if (_lines == nil || !_lines.count) {
        return NO;
    }

    // Check if the number of lines are within bounds
    if (_lines.count > kMaxMenuLines) {
        NSLog(@"Too many lines (%lu). Not in a menu", _lines.count);
        return NO;
    }

    NSString *pattern = nil;

    // Thaumistry has no pattern and uses bullets instead of greater than. Check window setup: Two buffer windows, one with only one line
    if (glkctl.gameID == kGameIsThaumistry) {
        if (![self checkForThaumistryMenu])
            return NO;
    } else {
        // Now we look for instructions pattern: " = ", "[N]ext" etc, depending on system
        if ([format isEqualToString:@"glulx"] || [format isEqualToString:@"zcode"]) {
            if (glkctl.gameID == kGameIsBeyondZork) {
                pattern = @"(Use the (↑ and ↓) keys(?s).+?(?=>))";
            } else if (glkctl.gameID == kGameIsAnchorheadOriginal) {
                pattern = @"\\[press (BACKSPACE) (to return to .+)\\]";
            } else {
                // First group: word before " = ". Second group: anything after " = "
                // until two spaces or newline or space + newline
                pattern = @"(\\w+) = (.+?(?=  |\\n| \\n|$))";
            }
        } else if ([format isEqualToString:@"hugo"]) {
            pattern = @"\\[((\\w+)].+?(?=  |\\n| \\n))";
        } else if ([format isEqualToString:@"tads3"]) {
            // First group: word before "=". Second group: anything after "="
            // until two spaces or newline or space + newline or word + "="
            pattern = @"(\\w+)=(.+?(?=  |\\n| \\n| \\w+=|$))";
        }

        _recheckNeeded = NO;
        _menuCommands = [self extractMenuCommandsUsingRegex:pattern];

        if (!_menuCommands.count) {
            // Extra check and hacks for Beyond Zork Function Key Definitions menu
            if (glkctl.gameID == kGameIsBeyondZork) {
                // Definitions menu
                _menuCommands = [self extractMenuCommandsUsingRegex:@"(Function (Key) Definitions)"];
                if (!_menuCommands.count) {
                    // DecSystem-20 but not VT220
                    _menuCommands = [self extractMenuCommandsUsingRegex:@"(Use the (UP and DOWN) arrow keys(?s).+?(?=>))"];
                    if (!_menuCommands.count) {
                        return NO;
                    }
                } else {
                    _menuCommands = @{ @"":@"" };
                    // If in the Definitions menu, add the last two lines
                    NSRange range = [_attrStr.string rangeOfString:@"Restore Defaults"];
                    if (range.length) {
                        [_lines addObject:[NSValue valueWithRange:range]];
                        range = [_attrStr.string rangeOfString:@"Exit"];
                        if (range.length) {
                            [_lines addObject:[NSValue valueWithRange:range]];
                        }
                    }
                }
            } else {
                if (glkctl.gameID == kGameIsVespers) {
                    // Vespers have some menus with instructions like "P=Go up", i.e. no spaces around the "="
                    _menuCommands = [self extractMenuCommandsUsingRegex:@"(\\w+)=(.+?(?=  |\\n| \\n|$))"];
                } else if ([format isEqualToString:@"zcode"]) {
                    // The Unforgotten has no instructions, so check for title
                    _menuCommands = [self extractMenuCommandsUsingRegex:@"(Unforgotten)\\s+(By Quintin Pan)"];
                    // Hack to skip the empty fourth line in The Unforgotten
                    if (_menuCommands.count)
                        [_lines removeObjectAtIndex:3];
                }
                if (!_menuCommands.count) {
                    // If we didn't find a pattern, decide this is not a menu
                    return NO;
                }
            }
        }

        if (_recheckNeeded) {
            _lines.array = [self recheckClusterStartingWithCharacter:initialChar];
            if (!_lines.count) {
                //Failed recheck. Give up.
                return NO;
            }
        }
    }

    // Now we check if the initial line is actually not part of the menu,
    // as seen in Vespers.
    // We assume this to be the case if the first has no leading spaces
    // but the next line has.
    if (_lines.count > 2 && !LeadingSpaces(_attrStr.string, _lines[0].rangeValue, YES) &&
        LeadingSpaces(_attrStr.string, _lines[1].rangeValue, YES)) {
        [_lines removeObjectAtIndex:0];
    }

    // Otherwise, look for >, *, •, change of color or reverse video
    _selectedLine = [self findSelectedLine];

    // If we find no currently selected line, decide this is not a menu
    if (_selectedLine == NSNotFound) {
        return NO;
    }

    return YES;
}

- (NSUInteger)findSelectedLine {
    if (_glkctl.showingInfocomV6Menu) {
        return _glkctl.infocomV6MenuHandler.selectedLine;
    }
    if (_lines.count == 1)
        return 0;
    NSUInteger guess = [self findLeadingSelectionCharacter];
    if (guess == NSNotFound) {
        guess = [self findTrailingSelectionCharacter];
        if (guess == NSNotFound) {
            guess = [self findAttributeSelection];
        }
    }
    return guess;
}

- (NSUInteger)findLeadingSelectionCharacter {
    NSUInteger guess = NSNotFound;
    BOOL foundGreaterThan = NO;
    unichar firstChar;
    for (NSUInteger i = 0; i < _lines.count; i++) {
        firstChar = [self firstNonspaceInLine:i];
        if (firstChar == '>' || firstChar == '*') {
            if (foundGreaterThan) {
                // Found more than one matching character
                return NSNotFound;
            } else {
                foundGreaterThan = YES;
                guess = i;
            }
        }
    }
    return guess;
}

- (NSUInteger)findTrailingSelectionCharacter {
    NSUInteger guess = NSNotFound;
    BOOL foundAsterisk = NO;
    unichar lastChar;
    for (NSUInteger i = 0; i < _lines.count; i++) {
        lastChar = [self lastNonspaceInLine:i];
        if (lastChar == '*') {
            if (foundAsterisk) {
                return NSNotFound;
            } else {
                foundAsterisk = YES;
                guess = i;
            }
        }
    }
    return guess;
}

- (NSUInteger)findAttributeSelection {
    NSUInteger __block guess = NSNotFound;
    NSRange lineRange = _lines.firstObject.rangeValue;
    NSRange menuRange = NSUnionRange(lineRange, _lines.lastObject.rangeValue);
    NSRange allText = NSMakeRange(0, _attrStr.length);
    menuRange = NSIntersectionRange(menuRange, allText);
    if (!_attrStr.length)
        return NSNotFound;
    NSDictionary *attrs = [_attrStr attributesAtIndex:0 effectiveRange:nil];
    NSColor *originalColor = attrs[NSBackgroundColorAttributeName];
    if (!originalColor)
        originalColor = TextViewOf(((NSTextStorage *)_attrStr).delegate).backgroundColor;
    [_attrStr
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:allText
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (value && ![value isEqualToColor:originalColor]) {
            NSRange intersection = NSIntersectionRange(menuRange, range);
            if (intersection.length && intersection.length < menuRange.length) { // Attribute change in menu range
                for (NSUInteger i = 0; i < _lines.count; i++)  {
                    NSRange thisLine = _lines[i].rangeValue;
                    NSRange overlap = NSIntersectionRange(range, thisLine);
                    if (overlap.length && overlap.length < thisLine.length)  { // Attribute change in line i
                        if (guess == NSNotFound || guess == i) {
                            guess = i;
                        } else {
                            // More than one match, bail
                            *stop = YES;
                            guess = NSNotFound;
                            break;
                        }
                    }
                    if (NSMaxRange(thisLine) > NSMaxRange(range))
                        break;
                }
            }
        }
    }];

    // In Tads 3 menus, the above code always picks the wrong one if there are two items.
    if (guess != NSNotFound && _isTads3 && _lines.count == 2) {
        if (guess == 0)
            guess = 1;
        else
            guess = 0;
    }

    // If no match, look for a unique foreground color
    // Does not work with less than 3 items except in TADS 3
    if (guess == NSNotFound && (_lines.count >= 3 || _isTads3)) {
        NSMutableArray *foregroundColors = [[NSMutableArray alloc] initWithCapacity:_lines.count];
        NSMutableArray *backgroundColors = [[NSMutableArray alloc] initWithCapacity:_lines.count];

        for (NSValue *rangeVal in _lines) {
            NSRange range = rangeVal.rangeValue;
            NSUInteger spaces = (NSUInteger)LeadingSpaces(_attrStr.string, range, NO);
            range = NSMakeRange(range.location + spaces, range.length - spaces);
            range = NSIntersectionRange(allText, range);
            if (range.location >= _attrStr.length) {
                [foregroundColors addObject:[NSNull null]];
                [backgroundColors addObject:[NSNull null]];
                continue;
            }
            NSDictionary *attrDict = [_attrStr attributesAtIndex:range.location effectiveRange:nil];

            NSColor *lineForeground = attrDict[NSForegroundColorAttributeName];
            [foregroundColors addObject:lineForeground ?: [NSNull null]];

            NSColor *lineBackground = attrDict[NSBackgroundColorAttributeName];
            [backgroundColors addObject:lineBackground ?: [NSNull null]];

            // TADS 3 menus lines usually begin with a ">" which is made invisible in non-selected lines
            // by giving the text and background the same color.
            // In a two-line TADS 3 menu, we assume that the line where foreground and background don't match
            // (and thus the ">" is visible) is the selected line.
            if (_isTads3 && _lines.count == 2 && lineBackground != nil && ![lineBackground isEqual:lineForeground]) {
                return [_lines indexOfObject:rangeVal];
            }
        }

        guess = UniqueColorIndex(foregroundColors);

        // If no match, look for a unique background color
        if (guess == NSNotFound)
            guess = UniqueColorIndex(backgroundColors);
    }

    return guess;
}

// What we call a cluster is a number of adjacent lines (more than one).
// delimited by blank lines or the beginning or end of the text.

- (NSArray *)findLargestClusterStartingWithCharacter:(unichar)startChar {
    NSArray *menuLines = @[];
    // Add the string of every Glk Window to an array
    _viewStrings = [[NSMutableArray alloc] init];
    GlkWindow *viewWithCluster;
    for (GlkWindow *view in _glkctl.gwindows.allValues) {
        NSString *viewString = TextViewOf(view).string;
        NSUInteger maxLength = [view isKindOfClass:[GlkTextGridWindow class]] ? kMaxGridTextLength : kMaxBufferTextLength;
        if (viewString.length > 0 && viewString.length < maxLength && view.frame.size.height > 0) {
            [_viewStrings addObject:viewString];
            NSArray *cluster = [self largestClusterInString:viewString lines:viewString.lineRanges startingWithCharacter:startChar];
            if (cluster.count > menuLines.count) {
                menuLines = cluster;
                viewWithCluster = view;
            }
        }
    }
    if (menuLines.count) {
        NSTextStorage *textStorage = TextViewOf(viewWithCluster).textStorage;
        if (!textStorage)
            textStorage = [[NSTextStorage alloc] init];
        _attrStr = textStorage;
    }
    return menuLines;
}

- (NSArray *)largestClusterInString:(NSString *)string lines:(NSArray<NSValue *> *)lines startingWithCharacter:(unichar)startChar {
    BOOL isHugo = [_glkctl.game.detectedFormat isEqualToString:@"hugo"];
    NSArray *menuLines = @[];
    NSUInteger line = 0;
    NSArray *currentCluster;
    NSArray *lastCluster = @[];
    do {
        currentCluster = [self findClusterInString:string andLines:lines startingWithCharacter:startChar atIndex:line];

        // Some games have a blank line as a divider in the middle of
        // the menu, so look for additional clusters and add these if found.
        if (currentCluster.count && lastCluster.count && [lines indexOfObject:lastCluster.lastObject] == line - 1 &&
            ABS(LeadingSpaces(string, [currentCluster.firstObject rangeValue], YES) -
                LeadingSpaces(string, [lastCluster.lastObject rangeValue], YES)) < 3 ) {
            if (!isHugo) {
                lastCluster = [lastCluster arrayByAddingObject:lines[line]];
            }
            currentCluster =
            [lastCluster arrayByAddingObjectsFromArray:currentCluster];
        }

        if (currentCluster.count > menuLines.count) {
            menuLines = currentCluster;
        }
        line = [lines indexOfObject:currentCluster.lastObject] + 1;
        lastCluster = currentCluster;
    } while (currentCluster.count);
    return menuLines;
}

- (NSArray *)findClusterInString:(NSString *)string andLines:(NSArray *)lines startingWithCharacter:(unichar)character atIndex:(NSUInteger)startPos {
    NSMutableArray *menulines = [[NSMutableArray alloc] init];
    if (!lines.count)
        return menulines;
    for (NSUInteger index = startPos; index < lines.count; index++) {
        // Check for leading char
        if ([self firstCharacterInArray:lines andIndex:index andString:string] != character &&
            [self firstCharacterInArray:lines andIndex:index - 1 andString:string] != character &&
            [self firstCharacterInArray:lines andIndex:index + 1 andString:string] != character) {
            // Line does not begin with requested character. Skip ahead to the next blank line
            menulines = [[NSMutableArray alloc] init];
            do {
                index++;
            } while (index < lines.count && ![string rangeIsEmpty:lines[index]]);
        } else if ([string rangeIsEmpty:lines[index]]) {
            if (menulines.count) {
                // Found empty line. Return results.
                return menulines;
            }
        } else {
            [menulines addObject:lines[index]];
        }
    }
    return menulines;
}

- (NSArray *)recheckClusterStartingWithCharacter:(unichar)startChar {

    NSString *string = _attrStr.string;
    NSMutableArray<NSValue *> *lines = _lines.mutableCopy;

    NSUInteger startIndex = NSMaxRange(lines.lastObject.rangeValue);

    // Re-add lines after detected cluster
    for (NSUInteger index = startIndex; index < string.length;) {
        NSRange linerange = [string lineRangeForRange:NSMakeRange(index, 0)];
        index = NSMaxRange(linerange);
        [lines addObject:[NSValue valueWithRange:linerange]];
    }

    // Strip trailing blank lines
    while (lines.count && [string rangeIsEmpty:lines.lastObject])
        [lines removeLastObject];

    return [self largestClusterInString:string lines:lines startingWithCharacter:startChar];
}

- (unichar)firstCharacterInArray:(NSArray<NSValue *> *)array andIndex:(NSUInteger)index andString:(NSString *)string {
    if (index >= array.count)
        return '\0';
    NSRange range = array[index].rangeValue;
    NSRange allText = NSMakeRange(0, string.length);
    range = NSIntersectionRange(allText, range);
    if (range.length == 0)
        return '\0';
    unichar first = [string characterAtIndex:range.location];
    if (first == 0xa0)
        first = ' ';
    return first;
}

- (unichar)firstNonspaceInLine:(NSUInteger)index {
    NSString *substring = [self trimSpacesFromLine:index];
    if (!substring.length)
        return '\0';
    return [substring characterAtIndex:0];
}

- (unichar)lastNonspaceInLine:(NSUInteger)index {
    NSString *substring = [self trimSpacesFromLine:index];
    if (!substring.length)
        return '\0';
    return [substring characterAtIndex:substring.length - 1];
}

- (NSString *)trimSpacesFromLine:(NSUInteger)index {
    if (index >= _lines.count)
        return @"";
    NSRange range = _lines[index].rangeValue;
    NSRange allText = NSMakeRange(0, _attrStr.string.length);
    range = NSIntersectionRange(allText, range);
    NSString *substring = [_attrStr.string substringWithRange:range];
    substring = [substring stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return substring;
}

- (NSDictionary *)extractMenuCommandsUsingRegex:(NSString *)regexString {
    NSMutableDictionary *menuDict = [[NSMutableDictionary alloc] init];
    NSMutableArray *keys = [[NSMutableArray alloc] init];
    NSRegularExpression *regex =
    [NSRegularExpression regularExpressionWithPattern:regexString
                                              options:0
                                                error:nil];
    for (NSString *string in _viewStrings) {
        if (string.length) {
            NSArray *matches =
            [regex matchesInString:string
                           options:0
                             range:NSMakeRange(0, string.length)];
            NSRange valueRange;
            NSString *key;
            for (NSTextCheckingResult *match in matches) {
                valueRange = [match rangeAtIndex:1];
                NSRange keyRange = [match rangeAtIndex:2];
                key = [string substringWithRange:keyRange];
                NSString *value = [string substringWithRange:valueRange];

                if (key.length) {
                    if (!menuDict[key])
                        [keys addObject:key];
                    menuDict[key] = value;
                }
            }
            if (matches.count) {
                if ([string isEqualToString:_attrStr.string]) {
                    // Remove the lines with instructions from the
                    // detected menu cluster
                    NSRange matchRange = ((NSTextCheckingResult *)matches.firstObject).range;
                    matchRange = NSUnionRange(matchRange, ((NSTextCheckingResult *)matches.lastObject).range);
                    NSUInteger lastOverlap = NSNotFound;
                    for (NSUInteger i = 0; i < _lines.count; i++) {
                        NSRange range = _lines[i].rangeValue;
                        NSRange intersect = NSIntersectionRange(range, matchRange);
                        if (intersect.length)
                            lastOverlap = i;
                        if (NSMaxRange(range) > NSMaxRange(matchRange))
                            break;
                    }
                    if (lastOverlap < _lines.count - 1) {
                        _recheckNeeded = YES;
                        _lines.array = [_lines subarrayWithRange:NSMakeRange(lastOverlap + 1, _lines.count - lastOverlap - 1)];
                        while (_lines.count && [string rangeIsEmpty:_lines.firstObject])
                            [_lines removeObjectAtIndex:0];
                    }
                }

                break;
            }
        }
    }
    if (keys.count) {
        _menuKeys = keys;
    }
    return menuDict;
}

- (BOOL)checkForThaumistryMenu {
    if (_glkctl.gwindows.count == 3) {
        GlkWindow *hidden = nil;
        GlkWindow *title = nil;
        GlkWindow *menu = nil;
        for (GlkWindow *view in _glkctl.gwindows.allValues) {
            NSTextView *textview = TextViewOf(view);
            if (!textview)
                continue;
            if (textview.textStorage == _attrStr) {
                menu = view;
            } else if (view.frame.size.height == 0) {
                hidden = view;
            } else {
                NSString *string = textview.textStorage.string;
                NSRange allText = NSMakeRange(0, string.length);
                NSRange firstLine = [string lineRangeForRange:NSMakeRange(0, 0)];
                if (NSEqualRanges(allText, firstLine))
                    title = view;
            }
        }
        if (title != nil && menu != nil && hidden != nil) {
            return YES;
        }
    }
    return NO;
}

- (void)speakSelectedLine {
    if (!_haveSpokenMenu) {
        [self performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:kFirstSpeakDelay];
    } else {
        [self performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:0];
    }
}

-(void)deferredSpeakSelectedLine:(id)sender {
    _selectedLine = [self findSelectedLine];
    if (_selectedLine == NSNotFound)
        return;
    NSString *selectedLineString;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    // We use the number of menu lines as a proxy to see if we have just switched to a new menu
    if (_lastNumberOfItems && _lastNumberOfItems != _lines.count && !_glkctl.showingInfocomV6Menu)
        _haveSpokenMenu = NO;
    _lastNumberOfItems = _lines.count;

    GlkController *glkctl = _glkctl;
    if (!_haveSpokenMenu) {
        _haveSpokenMenu = YES;
        [self speakString:[self menuLineStringWithTitle:YES index:YES total:YES instructions:YES]];
        return;
    } else if (sender == glkctl) {
        selectedLineString = [self menuLineStringWithIndex:YES total:YES instructions:YES];
    } else {
        selectedLineString = [self menuLineStringWithIndex:(glkctl.theme.vOSpeakMenu >= kVOMenuIndex) total:(glkctl.theme.vOSpeakMenu == kVOMenuTotal) instructions:NO];

        // Speak the instructions after 5 seconds, which is assumed to be long enough
        // to not interrupt the speaking of the selected line
        [self performSelector:@selector(speakInstructions:) withObject:nil afterDelay:kInstructionsDelay];

        // If we have chosen Quit from the Beyond Zork start menu,
        // this makes sure that the text "Are you sure you want
        // to leave the story now?" is spoken instead of the selected
        // menu line (which is still just "Quit".)
        if (glkctl.gameID == kGameIsBeyondZork && _lastSpokenString && [selectedLineString isEqualToString:_lastSpokenString]) {
            for (GlkWindow *view in glkctl.gwindows.allValues) {
                if ([view isKindOfClass:[GlkTextBufferWindow class]]) {
                    GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)view;
                    NSString *string = bufWin.textview.string;
                    if ([string rangeOfString:@"Are you sure you want to leave the story now?"].location != NSNotFound) {
                        [bufWin performSelector:@selector(repeatLastMove:) withObject:nil afterDelay:0.1];
                        return;
                    }
                }
            }
        }
        _lastSpokenString = selectedLineString;
    }
    [self speakString:selectedLineString];
}

- (NSString *)menuLineStringWithTitle:(BOOL)useTitle index:(BOOL)useIndex total:(BOOL)useTotal instructions:(BOOL)useInstructions {

    if (_glkctl.showingInfocomV6Menu) {
        return [_glkctl.infocomV6MenuHandler menuLineStringWithTitle:useTitle index:useIndex total:useTotal instructions:useInstructions];
    }

    NSString *menuLineString = [self menuLineStringWithIndex:useIndex total:useTotal instructions:useInstructions];
    if (useTitle) {
        NSString *titleString = [self constructMenuTitleString];
        if (titleString.length) {
            titleString = [NSString stringWithFormat:@"We are in a menu, \"%@.\"\n", titleString];
        } else {
            titleString = @"We are in a menu.\n";
        }
        menuLineString = [titleString stringByAppendingString:menuLineString];
    }
    return menuLineString;
}


- (NSString *)menuLineStringWithIndex:(BOOL)useIndex total:(BOOL)useTotal instructions:(BOOL)useInstructions {

    if (_glkctl.showingInfocomV6Menu) {
        return [_glkctl.infocomV6MenuHandler menuLineStringWithTitle:NO index:useIndex total:useTotal instructions:useInstructions];
    }

    NSRange selectedLineRange = _lines[_selectedLine].rangeValue;
    NSRange allText = NSMakeRange(0, _attrStr.length);
    selectedLineRange = NSIntersectionRange(allText, selectedLineRange);
    NSString *menuItemString = [_attrStr.string substringWithRange:selectedLineRange];

    menuItemString = [SelectionMarkerRegex() stringByReplacingMatchesInString:menuItemString
                                                         options:0
                                                           range:NSMakeRange(0, menuItemString.length)
                                                    withTemplate:@""];
    NSString *trimmedString = [menuItemString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmedString.length == 0)
        menuItemString = @"Empty line.";

    // Add pre-loaded input field text to Beyond Zork definitions menu
    if (_glkctl.gameID == kGameIsBeyondZork) {
        id delegate = ((NSTextStorage *)_attrStr).delegate;
        if ([delegate isKindOfClass:[GlkTextGridWindow class]] && ((GlkTextGridWindow *)delegate).input) {
            menuItemString = [menuItemString stringByAppendingString:((GlkTextGridWindow *)delegate).enteredTextSoFar];
        }
    }

    if (useIndex) {
        NSString *indexString = [NSString stringWithFormat:@".\nMenu item %lu", _selectedLine + 1];
        if (useTotal) {
            indexString = [indexString stringByAppendingString:
                           [NSString stringWithFormat:@" of %lu", _lines.count]];
        }
        indexString = [indexString stringByAppendingString:@".\n"];
        menuItemString = [menuItemString stringByAppendingString:indexString];
    }

    if (useInstructions) {
        menuItemString = [menuItemString stringByAppendingString:[self constructMenuInstructionString]];
    }

    return menuItemString;
}

- (void)speakInstructions:(id)sender {
    [_glkctl announce:[self constructMenuInstructionString] priority:NSAccessibilityPriorityLow];
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0)
        return;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    if (_glkctl.gameID == kGameIsBeyondZork) {
        // Delete graphic indicator
        string = [BeyondZorkIndicatorRegex() stringByReplacingMatchesInString:string
                                                     options:0
                                                       range:NSMakeRange(0, string.length)
                                                withTemplate:@""];

    }

    [_glkctl speakStringNow:string];
}

- (NSString *)constructMenuInstructionString {

    if (_glkctl.showingInfocomV6Menu) {
        return [_glkctl.infocomV6MenuHandler constructMenuInstructionString];
    }

    NSString *string = @"";
    if (_glkctl.gameID == kGameIsBeyondZork) {
        string = _menuCommands[_menuKeys.firstObject];
        if (!string.length)
            return @"";
        string = [string stringByReplacingOccurrencesOfString:@"↑" withString:@"UP ARROW"];
        string = [string stringByReplacingOccurrencesOfString:@"↓" withString:@"DOWN ARROW"];
        string = [string stringByReplacingOccurrencesOfString:@"←" withString:@"LEFT ARROW"];
        string = [string stringByReplacingOccurrencesOfString:@"→" withString:@"RIGHT ARROW"];
        string = [string stringByAppendingString:@"\n"];
    } else {
        for (NSString *key in _menuKeys) {

            // Remove brackets in result
            // First the entire word "Enter] "
            // It might be overkill to use a regex here,
            NSString *valueString = (NSString *)_menuCommands[key];
            valueString =
            [BracketedWordRegex() stringByReplacingMatchesInString:valueString
                                                 options:0
                                                   range:NSMakeRange(0, valueString.length)
                                            withTemplate:@""];
            //Then any brackets in things like "N]ext"
            valueString = [valueString stringByReplacingOccurrencesOfString:@"]" withString:@""];

            valueString = valueString.capitalizedString;

            NSString *command = [NSString stringWithFormat:@"%@: \"%@\".\n", valueString, key];
            string = [string stringByAppendingString:command];
        }
    }
    return string;
}

- (NSString *)constructMenuTitleString {
    NSString *string = @"";
    NSTextView *topTextView = nil;
    CGFloat topPos = CGFLOAT_MAX;
    // Find upper window. The game view is flipped, so it has the smallest y.
    for (GlkWindow *view in _glkctl.gwindows.allValues) {
        NSTextView *textview = TextViewOf(view);
        if (textview.textStorage.length && view.frame.size.height > 0) {
            if (view.frame.origin.y < topPos) {
                topTextView = textview;
                topPos = view.frame.origin.y;
            }
        }
    }
    string = topTextView.textStorage.string;
    string = [string componentsSeparatedByString:@"\n"].firstObject;

    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([string rangeOfString:@"    "].location != NSNotFound)
        string = @"";
    if (_glkctl.gameID == kGameIsBeyondZork && string.length == 0) {
        NSRange range = _lines.firstObject.rangeValue;
        NSString *topString = [_attrStr.string substringWithRange:range];
        topString = [topString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([topString isEqualToString:@"Begin using a preset character"])
            string = @"Character Setup";
        else if ([topString isEqualToString:@"F1"])
            string = @"Function Key Definitions";
    }
    return string;
}

@end

//
//  ZMenu.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import "ZMenu.h"

#import "GlkController.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkGraphicsWindow.h"
#import "Game.h"
#import "Theme.h"
#import "InfocomV6MenuHandler.h"

#import "NSColor+integer.h"
#import "NSString+Categories.h"

@implementation ZMenu

- (instancetype)initWithGlkController:(GlkController *)glkctl {
    self = [super init];
    if (self) {
        _glkctl = glkctl;
    }
    return self;
}

- (BOOL)isMenu {
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

    //This should be checked in convertToLines instead
    NSUInteger maxLines = 25;

    // Check if the number of lines are within bounds
    if (_lines.count > maxLines) {
        NSLog(@"Too many lines (%ld). Not in a menu", _lines.count);
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
    if (_lines.count > 2 && ![self leftMarginInRange:_lines.firstObject andString:_attrStr.string] &&
        [self leftMarginInRange:_lines[1] andString:_attrStr.string]) {
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
        originalColor = ((GlkTextBufferWindow *)((NSTextStorage *)_attrStr).delegate).textview.backgroundColor;
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
            NSUInteger spaces = (NSUInteger)[self initialSpacesInRange:rangeVal];
            range = NSMakeRange(range.location + spaces, range.length - spaces);
            range = NSIntersectionRange(allText, range);
            NSDictionary *attrDict = [_attrStr attributesAtIndex:range.location effectiveRange:nil];

            NSColor *lineForeground = attrDict[NSForegroundColorAttributeName];
            if (lineForeground == nil)
                [foregroundColors addObject:[NSNull null]];
            else
                [foregroundColors addObject:lineForeground];

            NSColor *lineBackground = attrDict[NSBackgroundColorAttributeName];
            if (lineBackground == nil)
                [backgroundColors addObject:[NSNull null]];
            else
                [backgroundColors addObject:lineBackground];

            // TADS 3 menus lines usually begin with a ">" which is made invisible in non-selected lines
            // by giving the text and background the same color.
            // In a two-line TADS 3 menu, we assume that the line where foreground and background don't match
            // (and thus the ">" is visible) is the selected line.
            if (_isTads3 && _lines.count == 2 && lineBackground != nil && ![lineBackground isEqual:lineForeground]) {
                return [_lines indexOfObject:rangeVal];
            }
        }
        NSCountedSet *colorsSet = [[NSCountedSet alloc] initWithArray:foregroundColors];

        for (NSColor *color in colorsSet) {
            if ([colorsSet countForObject:color] == 1)  {
                if (guess == NSNotFound) {
                    guess = [foregroundColors indexOfObject:color];
                    //Found unique foreground color
                } else {
                    guess = NSNotFound;
                    break;
                }
            }
        }

        // If no match, look for a unique background color
        if (guess == NSNotFound) {
            colorsSet = [[NSCountedSet alloc] initWithArray:backgroundColors];

            for (NSColor *color in colorsSet) {
                if ([colorsSet countForObject:color] == 1)  {
                    if (guess == NSNotFound) {
                        guess = [backgroundColors indexOfObject:color];
                        //Found unique background color
                    } else {
                        guess = NSNotFound;
                        break;
                    }
                }
            }
        }
    }

    return guess;
}

// What we call a cluster is a number of adjacent lines (more than one).
// delimited by blank lines or the beginning or end of the text.

- (NSArray *)findLargestClusterStartingWithCharacter:(unichar)startChar {
    NSArray *menuLines = @[];
    // Add the string of every Glk Window to an array
    _viewStrings = [[NSMutableArray alloc] init];
    NSString *viewString;
    GlkWindow *viewWithCluster;
    GlkController *glkctl = _glkctl;
    BOOL isHugo = [glkctl.game.detectedFormat isEqualToString:@"hugo"];
    for (GlkWindow *view in glkctl.gwindows.allValues) {
        if ([view isKindOfClass:[GlkTextGridWindow class]] || [view isKindOfClass:[GlkTextBufferWindow class]]) {
            viewString = ((GlkTextBufferWindow *)view).textview.string;
        } else {
            viewString = nil;
        }
        NSUInteger maxLength;
        if ([view isKindOfClass:[GlkTextGridWindow class]]) {
            maxLength = 10000;
        } else {
            maxLength = 4000;
        }
        if (viewString.length > 0 && viewString.length < maxLength && view.frame.size.height > 0) {
            [_viewStrings addObject:viewString];
            NSArray *lines = viewString.lineRanges;
            NSUInteger line = 0;
            NSArray *currentCluster;
            NSArray *lastCluster = @[];
            do {
                currentCluster = [self findClusterInString:viewString andLines:lines startingWithCharacter:startChar atIndex:line];

                // Some games have a blank line as a divider in the middle of
                // the menu, so look for additional clusters and add these if found.
                if (currentCluster.count && lastCluster.count && [lines indexOfObject:lastCluster.lastObject] == line - 1 &&
                    ABS([self leftMarginInRange:currentCluster.firstObject andString:viewString] -
                        [self leftMarginInRange:lastCluster.lastObject andString:viewString]) < 3 ) {
                    if (!isHugo) {
                        lastCluster = [lastCluster arrayByAddingObject:lines[line]];
                    }
                    currentCluster =
                    [lastCluster arrayByAddingObjectsFromArray:currentCluster];
                }

                if (currentCluster.count > menuLines.count) {
                    menuLines = currentCluster;
                    viewWithCluster = view;
                }
                line = [lines indexOfObject:currentCluster.lastObject] + 1;
                lastCluster = currentCluster;
            } while (currentCluster.count);
        }
    }
    if (menuLines.count) {
        NSTextStorage *textStorage = ((GlkTextBufferWindow *)viewWithCluster).textview.textStorage;
        if (!textStorage)
            textStorage = [[NSTextStorage alloc] init];
        _attrStr = textStorage;
    }
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

// FIXME this is a lot of ugly code duplication
- (NSArray *)recheckClusterStartingWithCharacter:(unichar)startChar {

    NSString *string = _attrStr.string;
    NSMutableArray<NSValue *> *lines = _lines.mutableCopy;

    NSUInteger startIndex = NSMaxRange(lines.lastObject.rangeValue);

    NSArray *menuLines = @[];

    // Re-add lines after detected cluster
    for (NSUInteger index = startIndex; index < string.length;) {
        NSRange linerange = [string lineRangeForRange:NSMakeRange(index, 0)];
        index = NSMaxRange(linerange);
        [lines addObject:[NSValue valueWithRange:linerange]];
    }

    // Strip trailing blank lines
    BOOL lastLineBlank;
    do {
        lastLineBlank = [string rangeIsEmpty:lines.lastObject];
        if (lastLineBlank) {
            [lines removeLastObject];
        }
    } while (lastLineBlank);

    NSUInteger line = 0;
    NSArray *currentCluster;
    NSArray *lastCluster = @[];

    do {
        currentCluster = [self findClusterInString:string andLines:lines startingWithCharacter:startChar atIndex:line];

        // Some games have a blank lines as a dividers in the middle of
        // the menu, so look for additional clusters and add these if found.
        if (currentCluster.count && lastCluster.count && [lines indexOfObject:lastCluster.lastObject] == line - 1 &&
            ABS([self leftMarginInRange:currentCluster.firstObject andString:string] -
                [self leftMarginInRange:lastCluster.lastObject andString:string]) < 3 ) {
            if (![_glkctl.game.detectedFormat isEqualToString:@"hugo"]) {
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

- (unichar)firstCharacterInArray:(NSArray<NSValue *> *)array andIndex:(NSUInteger)index andString:(NSString *)string {
    if (index > array.count - 1)
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

// Returns the length of any prefix consisting of space characters (including '>')
- (NSInteger)leftMarginInRange:(NSValue *)rangeValue andString:(NSString *)string {
    NSRange range = rangeValue.rangeValue;
    NSRange allText = NSMakeRange(0, string.length);
    range = NSIntersectionRange(allText, range);
    NSUInteger spaces = 0;
    unichar c = [string characterAtIndex:range.location];
    while ((c == ' ' || c == 0xa0 || c == '>') && NSLocationInRange(range.location + spaces + 1, range)) {
        spaces++;
        c = [string characterAtIndex:range.location + spaces];
    }
    return (NSInteger)spaces;
}

// This does the same as the method above but does not
// count ">" as a space
- (NSInteger)initialSpacesInRange:(NSValue *)rangeValue {
    NSRange range = rangeValue.rangeValue;
    NSString *string = _attrStr.string;
    NSRange allText = NSMakeRange(0, string.length);
    range = NSIntersectionRange(allText, range);
    NSUInteger spaces = 0;
    unichar c = [string characterAtIndex:range.location];
    while ((c == ' ' || c == 0xa0) && NSLocationInRange(range.location + spaces + 1, range)) {
        spaces++;
        c = [string characterAtIndex:range.location + spaces];
    }
    return (NSInteger)spaces;
}

- (NSDictionary *)extractMenuCommandsUsingRegex:(NSString *)regexString {
    NSMutableDictionary *menuDict = [[NSMutableDictionary alloc] init];
    NSMutableArray *keys = [[NSMutableArray alloc] init];
    NSError *error = NULL;
    NSRegularExpression *regex =
    [NSRegularExpression regularExpressionWithPattern:regexString
                                              options:0
                                                error:&error];
    for (NSString *string in _viewStrings) {
        if ([string isNotEqualTo:[NSNull null]] && string.length) {
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
                    menuDict[key] = value;
                    [keys addObject:key];
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
                        while ([string rangeIsEmpty:_lines.firstObject] && _lines.count)
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
        GlkTextBufferWindow *hidden = nil;
        GlkTextBufferWindow *title = nil;
        GlkTextBufferWindow *menu = nil;
        for (GlkTextBufferWindow *view in _glkctl.gwindows.allValues) {
            if (view.textview.textStorage == _attrStr) {
                menu = view;
            } else if (view.frame.size.height == 0) {
                hidden = view;
            } else {
                NSString *string = view.textview.textStorage.string;
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
        [self performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:1.5];
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
        [self performSelector:@selector(speakInstructions:) withObject:nil afterDelay:5];

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

    NSRegularExpression *trimRegEx =
    [NSRegularExpression regularExpressionWithPattern:@"^\\s*[>\\*•](?: |)"
                                              options:0
                                                error:nil];
    menuItemString = [trimRegEx stringByReplacingMatchesInString:menuItemString
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
        NSString *indexString = [NSString stringWithFormat:@".\nMenu item %ld", _selectedLine + 1];
        if (useTotal) {
            indexString = [indexString stringByAppendingString:
                           [NSString stringWithFormat:@" of %ld", _lines.count]];
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
    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityLow),
        NSAccessibilityAnnouncementKey : [self constructMenuInstructionString]
    };

    NSAccessibilityPostNotificationWithUserInfo(
                                                _glkctl.window,
                                                NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0)
        return;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    if (_glkctl.gameID == kGameIsBeyondZork) {
        // Delete graphic indicator
        NSRegularExpression *trimRegEx =
        [NSRegularExpression regularExpressionWithPattern:@"X[O-W]{13}Y"
                                                  options:0
                                                    error:nil];
        string = [trimRegEx stringByReplacingMatchesInString:string
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
            NSError *error;
            NSRegularExpression *noBrackets =
            [NSRegularExpression regularExpressionWithPattern:@"\\w+]\\s"
                                                      options:0
                                                        error:&error];
            valueString =
            [noBrackets stringByReplacingMatchesInString:valueString
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
    GlkTextBufferWindow *topWindow = nil;
    NSUInteger topPos = NSNotFound;
    // Find upper window
    for (GlkTextBufferWindow *view in _glkctl.gwindows.allValues) {
        if ([view isKindOfClass:[GlkGraphicsWindow class]])
            continue;
        if (view.textview.textStorage.length && view.frame.size.height > 0) {
            if (view.frame.origin.y < topPos) {
                topWindow = view;
                topPos = (NSUInteger)view.frame.origin.y;
            }
        }
    }
    string = topWindow.textview.textStorage.string;
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

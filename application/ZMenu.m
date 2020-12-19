//
//  ZMenu.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import "ZMenu.h"

#import "GlkController.h"
#import "GlkWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkGraphicsWindow.h"

#import "Game.h"

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
    NSString *format = _glkctl.game.detectedFormat;
    unichar initialChar;

    if ([format isEqualToString:@"glulx"] || [format isEqualToString:@"zcode"] || [format isEqualToString:@"hugo"]) {
        initialChar = ' ';
    } else if ([format isEqualToString:@"tads3"]) {
        // If Tads 3: look for a cluster of lines where all start with a '>'.
        // '•' is only used in Thaumistry: Charm's Way.
        if (_glkctl.thaumistry)
            initialChar = u'•';
        else
            initialChar = '>';
    } else return NO; // We only support menus in Glulx or Zcode or Hugo or Tads 3

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
    if (_glkctl.thaumistry) {
        if (![self checkForThaumistryMenu])
            return NO;
    } else {
        // Now we look for instructions pattern: " = ", "[N]ext" etc, depending on system
        if ([format isEqualToString:@"glulx"] || [format isEqualToString:@"zcode"]) {
            if (_glkctl.beyondZork) {
                pattern = @"(Use the (↑ and ↓) keys.+)";
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

        _menuCommands = [self extractMenuCommandsUsingRegex:pattern];

        if (!_menuCommands.count) {
            // Extra check and hacks for Beyond Zork Function Key Definitions menu
            if (_glkctl.beyondZork) {
                _menuCommands = [self extractMenuCommandsUsingRegex:@"(Function (Key) Definitions)"];
                if (!_menuCommands.count) {
                    NSLog(@"Found no menu commands. Not a menu");
                    return NO;
                } else {
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
                // If we don't find the pattern, decide this is not a menu
            } else return NO;
        }

        //        for (NSString *key in _menuKeys)
        //            NSLog(@"\"%@\" : \"%@\"", key, _menuCommands[key]);
    }

    // Otherwise, look for >, *, •, change of color or reverse video
    _selectedLine = [self findSelectedLine];

    // If we find no currently selected line, decide this is not a menu
    if (_selectedLine == NSNotFound) {
        NSLog(@"Found no selected line. Not a menu");
        return NO;
    }

    //    NSLog(@"We are in a menu!");
    //    NSLog(@"It has %ld lines:", _lines.count);
    //    NSRange allText = NSMakeRange(0, _attrStr.length);
    //    for (NSValue *rangeValue in _lines) {
    //        NSRange range = rangeValue.rangeValue;
    //        range = NSIntersectionRange(allText, range);
    //        NSString *line = [_attrStr.string substringWithRange:range];
    //        NSLog(@"%@", line);
    //    }
    //    NSRange range = ((NSValue *)_lines[_selectedLine]).rangeValue;

    //    NSLog(@"Currently selected line: %ld (\"%@\")", _selectedLine, [_attrStr.string substringWithRange:range]);
    return YES;
}

- (NSUInteger)findSelectedLine {
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
    __block NSUInteger guess = NSNotFound;
    NSRange lineRange = ((NSValue *)_lines.firstObject).rangeValue;
    NSRange menuRange = NSUnionRange(lineRange, ((NSValue *)_lines.lastObject).rangeValue);
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
            //            NSLog(@"Background Color Attribute change, color %@, range %@", (NSColor *)value, NSStringFromRange(range));
            NSRange intersection = NSIntersectionRange(menuRange, range);
            if (intersection.length && intersection.length < menuRange.length) { // Attribute change in menu range
                for (NSUInteger i = 0; i < _lines.count; i++)  {
                    NSRange thisLine = ((NSValue *)_lines[i]).rangeValue;
                    NSRange overlap = NSIntersectionRange(range, thisLine);
                    if (overlap.length && overlap.length < thisLine.length)  { // Attribute change in line i
                        if (guess == NSNotFound || guess == i) {
                            guess = i;
                            // NSLog(@"Found background color change in line %ld", i);
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
    if (guess != NSNotFound && [_glkctl.game.detectedFormat isEqualToString:@"tads3"] && _lines.count == 2) {
        if (guess == 0)
            guess = 1;
        else
            guess = 0;
    }

    // If no match, look for a unique foreground color
    if (guess == NSNotFound && _lines.count >= 3) { // Does not work with less than 3 items
        NSMutableArray *foregroundColors = [[NSMutableArray alloc] initWithCapacity:_lines.count];
        NSMutableArray *backgroundColors = [[NSMutableArray alloc] initWithCapacity:_lines.count];

        for (NSValue *rangeVal in _lines) {
            NSRange range = rangeVal.rangeValue;
            NSUInteger spaces = (NSUInteger)[self numberOfInitialSpacesInRange:rangeVal andString:_attrStr.string];
            range = NSMakeRange(range.location + spaces, range.length - spaces);
            range = NSIntersectionRange(allText, range);
            NSDictionary *attrDict = [_attrStr attributesAtIndex:range.location effectiveRange:nil];

            NSColor *lineColor = attrDict[NSForegroundColorAttributeName];
            if (lineColor == nil)
                [foregroundColors addObject:[NSNull null]];
            else
                [foregroundColors addObject:lineColor];

            lineColor = attrDict[NSBackgroundColorAttributeName];
            if (lineColor == nil)
                [backgroundColors addObject:[NSNull null]];
            else
                [backgroundColors addObject:lineColor];
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
    for (GlkWindow *view in _glkctl.gwindows.allValues) {
        if ([view isKindOfClass:[GlkTextGridWindow class]]) {
            viewString = ((GlkTextGridWindow *)view).textview.string;
        } else if ([view isKindOfClass:[GlkTextBufferWindow class]]) {
            viewString = ((GlkTextBufferWindow *)view).textview.string;
        } else {
            viewString = nil;
        }
        if (viewString && view.frame.size.height > 0 && viewString.length < 4000) {
            [_viewStrings addObject:viewString];
            NSArray *lines = [self convertToLines:viewString];
            NSUInteger line = 0;
            NSArray *currentCluster;
            NSArray *lastCluster = @[];
            do {
                currentCluster = [self findClusterInString:viewString andLines:lines startingWithCharacter:startChar atIndex:line];

                // Some games have a blank lines as a dividers in the middle of
                // the menu, so look for additional clusters and add these if found.
                if (currentCluster.count && lastCluster.count && [lines indexOfObject:lastCluster.lastObject] == line - 1 &&
                    ABS([self numberOfInitialSpacesInRange:currentCluster.firstObject andString:viewString] -
                        [self numberOfInitialSpacesInRange:lastCluster.lastObject andString:viewString]) < 3 ) {
                    if (![_glkctl.game.detectedFormat isEqualToString:@"hugo"]) {
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
        NSTextStorage *textStorage = ((GlkTextGridWindow *)viewWithCluster).textview.textStorage;
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
            } while (index < lines.count && ![self rangeIsEmpty:lines[index] inString:string]);
        } else if ([self rangeIsEmpty:lines[index] inString:string]) {
            if (menulines.count) {
                //Found empty line. Return results.
                return menulines;
            }
        } else {
            [menulines addObject:lines[index]];
        }
    }
    return menulines;
}

- (unichar)firstCharacterInArray:(NSArray *)array andIndex:(NSUInteger)index andString:(NSString *)string {
    if (index > array.count - 1)
        return '\0';
    NSRange range = ((NSValue *)array[index]).rangeValue;
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
    NSRange range = ((NSValue *)_lines[index]).rangeValue;
    NSRange allText = NSMakeRange(0, _attrStr.string.length);
    range = NSIntersectionRange(allText, range);
    NSString *substring = [_attrStr.string substringWithRange:range];
    substring = [substring stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return substring;
}

- (NSInteger)numberOfInitialSpacesInRange:(NSValue *)rangeValue andString:(NSString *)string {
    NSRange range = rangeValue.rangeValue;
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

- (BOOL)rangeIsEmpty:(NSValue *)rangeValue inString:(NSString *)string {
    if (!string || !string.length)
        return YES;
    NSRange range = rangeValue.rangeValue;
    NSRange allText = NSMakeRange(0, string.length);
    range = NSIntersectionRange(allText, range);
    NSString *trimmedString = [string substringWithRange:range];
    trimmedString = [trimmedString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return (trimmedString.length == 0);
}

- (NSArray *)convertToLines:(NSString *)string {
    NSUInteger stringLength = string.length;
    NSRange linerange;
    NSMutableArray *lines = [[NSMutableArray alloc] init];
    for (NSUInteger index = 0; index < stringLength;) {
        // This includes newlines
        linerange = [string lineRangeForRange:NSMakeRange(index, 0)];
        index = NSMaxRange(linerange);
        [lines addObject:[NSValue valueWithRange:linerange]];
    }

    if (lines.count) {
        //Strip leading blank lines
        BOOL firstLineBlank;
        do {
            firstLineBlank = [self rangeIsEmpty:lines.firstObject inString:string];
            if (firstLineBlank) {
                [lines removeObjectAtIndex:0];
                // If all lines are blank, we are not in a menu
                if (!lines.count)
                    return lines;
            }
        } while (firstLineBlank);

        //Strip trailing blank lines
        BOOL lastLineBlank;
        do {
            lastLineBlank = [self rangeIsEmpty:lines.lastObject inString:string];
            if (lastLineBlank) {
                [lines removeLastObject];
            }
        } while (lastLineBlank);
    }
    return lines;
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
            //            NSLog(@"found %ld matches", matches.count);
            for (NSTextCheckingResult *match in matches) {
                NSRange valueRange = [match rangeAtIndex:1];
                NSRange keyRange = [match rangeAtIndex:2];
                NSString *key = [string substringWithRange:keyRange];
                NSString *value = [string substringWithRange:valueRange];

                if (key.length) {
                    menuDict[key] = value;
                    [keys addObject:key];
                }
            }
            if (matches.count) {
                if ([string isEqualToString:_attrStr.string]) {
                    NSRange matchRange = ((NSTextCheckingResult *)matches.firstObject).range;
                    matchRange = NSUnionRange(matchRange, ((NSTextCheckingResult *)matches.lastObject).range);
                    NSUInteger lastOverlap = NSNotFound;
                    for (NSUInteger i = 0; i < _lines.count; i++) {
                        NSRange range = ((NSValue *)_lines[i]).rangeValue;
                        NSRange intersect = NSIntersectionRange(range, matchRange);
                        if (intersect.length)
                            lastOverlap = i;
                        if (NSMaxRange(range) > NSMaxRange(matchRange))
                            break;
                    }
                    if (lastOverlap < _lines.count - 1) {
                        _lines.array = [_lines subarrayWithRange:NSMakeRange(lastOverlap + 1, _lines.count - lastOverlap - 1)];
                        while ([self rangeIsEmpty:_lines.firstObject inString:string] && _lines.count)
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
    [self performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:0.2];
}

-(void)deferredSpeakSelectedLine:(id)sender {
    _selectedLine = [self findSelectedLine];
    if (_selectedLine == NSNotFound)
        return;
    NSString *selectedLineString = [NSString stringWithFormat:@"On menu item %ld of %ld:\n", _selectedLine + 1, _lines.count];
    NSRange selectedLineRange = ((NSValue *)_lines[_selectedLine]).rangeValue;
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

    selectedLineString = [selectedLineString stringByAppendingString:menuItemString];
    if (!_haveSpokenMenu) {
        NSString *titleString = [self constructMenuTitleString];
        if (titleString.length) {
            titleString = [NSString stringWithFormat:@"We are in a menu, \"%@.\"\n", titleString];
        } else {
            titleString = @"We are in a menu.\n";
        }
        selectedLineString = [titleString stringByAppendingString:selectedLineString];
        _haveSpokenMenu = YES;
    }

    if (_glkctl.beyondZork) {
        id delegate = ((NSTextStorage *)_attrStr).delegate;
        if ([delegate isKindOfClass:[GlkTextGridWindow class]] && ((GlkTextGridWindow *)delegate).input) {
            selectedLineString = [selectedLineString stringByAppendingString:((GlkTextGridWindow *)delegate).enteredTextSoFar];
        }
    }

    [self speakString:selectedLineString];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSelector:@selector(speakInstructions:) withObject:nil afterDelay:5];
}

- (void)speakInstructions:(id)sender {
    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityLow),
        NSAccessibilityAnnouncementKey : [self constructMenuInstructionString]
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
//    [self performSelector:@selector(speakEscape:) withObject:nil afterDelay:6];
}

- (void)speakEscape:(id)sender {
    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityLow),
        NSAccessibilityAnnouncementKey : @"If this is NOT a menu, press ESCAPE to DISMISS."
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
    }
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0)
        return;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    if (_glkctl.beyondZork) {
        // Delete graphic inidicator
        NSRegularExpression *trimRegEx =
        [NSRegularExpression regularExpressionWithPattern:@"X[O-W]{13}Y"
                                                  options:0
                                                    error:nil];
        string = [trimRegEx stringByReplacingMatchesInString:string
                                                     options:0
                                                       range:NSMakeRange(0, string.length)
                                                withTemplate:@""];

    }

    NSDictionary *announcementInfo = @{
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        NSAccessibilityAnnouncementKey : string
    };

    NSWindow *mainWin = [NSApp mainWindow];

    if (mainWin) {
        NSAccessibilityPostNotificationWithUserInfo(
                                                    mainWin,
                                                    NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
        //        NSLog(@"speakString: \"%@\"", string);
    }
}

- (NSString *)constructMenuInstructionString {
    NSString *string = @"";
    if (_glkctl.beyondZork) {
        string = _menuCommands[_menuKeys.firstObject];
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
    if (self.glkctl.beyondZork && string.length == 0) {
        NSRange range = ((NSValue *)_lines.firstObject).rangeValue;
        NSString *topString = [_attrStr.string substringWithRange:range];
        topString = [topString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([topString isEqualToString:@"Begin using a preset character"])
            string = @"Character Setup";
    }
    return string;
}

@end

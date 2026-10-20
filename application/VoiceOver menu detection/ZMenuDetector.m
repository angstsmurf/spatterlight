//
//  ZMenuDetector.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-15 (as part of ZMenu).
//

#import "ZMenuDetector.h"

#import "NSColor+integer.h"
#import "NSString+Categories.h"
#import "DebugLog.h"

// Larger windows are not searched for menus.
static const NSUInteger kMaxGridTextLength = 10000;
static const NSUInteger kMaxBufferTextLength = 4000;
// A cluster of more lines than this is not a menu.
static const NSUInteger kMaxMenuLines = 25;

#pragma mark - Instruction patterns

// In each pattern the first group is the command and the second the key.

// First group: word before " = ". Second group: anything after " = "
// until two spaces or newline or space + newline.
static NSString * const kEqualsPattern = @"(\\w+) = (.+?(?=  |\\n| \\n|$))";
// Vespers has some menus with instructions like "P=Go up", i.e. no spaces
// around the "=".
static NSString * const kVespersPattern = @"(\\w+)=(.+?(?=  |\\n| \\n|$))";
// As above, also ending before the next word + "=".
static NSString * const kTads3Pattern = @"(\\w+)=(.+?(?=  |\\n| \\n| \\w+=|$))";
static NSString * const kHugoPattern = @"\\[((\\w+)].+?(?=  |\\n| \\n))";
static NSString * const kAnchorheadPattern = @"\\[press (BACKSPACE) (to return to .+)\\]";
static NSString * const kBeyondZorkPattern = @"(Use the (↑ and ↓) keys(?s).+?(?=>))";
static NSString * const kBeyondZorkDefinitionsPattern = @"(Function (Key) Definitions)";
// DecSystem-20 but not VT220.
static NSString * const kBeyondZorkDecSystemPattern = @"(Use the (UP and DOWN) arrow keys(?s).+?(?=>))";
// The Unforgotten has no instructions, so look for the title.
static NSString * const kUnforgottenPattern = @"(Unforgotten)\\s+(By Quintin Pan)";

#pragma mark - Helpers

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

// The first character of line index, with a non-breaking space read as a
// space, or '\0' if there is no such line.
static unichar FirstCharacter(NSString *string, NSArray<NSValue *> *lines, NSUInteger index) {
    if (index >= lines.count)
        return '\0';
    NSRange range = NSIntersectionRange(NSMakeRange(0, string.length), lines[index].rangeValue);
    if (range.length == 0)
        return '\0';
    unichar first = [string characterAtIndex:range.location];
    if (first == 0xa0)
        first = ' ';
    return first;
}

#pragma mark - Per-game rules

@class ZMenuDetection;

// How menus are recognised in one format, or in one game. The first rule
// that matches the game is used.
@interface ZMenuRule : NSObject
// nil matches any format, kGameIsGeneric any game.
@property (nullable) NSSet<NSString *> *formats;
@property kGameIdentity gameID;
// The game has no menus to find.
@property BOOL noMenus;
// Menu lines start with this character, or are next to one that does.
@property unichar initialChar;
// Instruction patterns, tried in order until one matches. A menu needs
// instructions unless this is nil.
@property (nullable) NSArray<NSString *> *patterns;
// A further test the windows must pass.
@property (nullable) BOOL (*extraCheck)(ZMenuDetection *menu, NSArray<ZMenuWindowText *> *windows);
// Called with the index of the pattern that matched.
@property (nullable) void (*afterMatch)(ZMenuDetection *menu, NSUInteger patternIndex);
@end

@implementation ZMenuRule
@end

@interface ZMenuDetection ()
@property NSAttributedString *text;
@property (nullable) NSColor *backgroundColor;
@property NSMutableArray<NSValue *> *menuLines;
@property NSDictionary<NSString *, NSString *> *commands;
@property NSArray<NSString *> *keys;
@property BOOL isTads3;
@property BOOL isHugo;
@property BOOL isBeyondZork;
@property NSUInteger selectedLine;
// The texts of the windows that were searched, one per window.
@property NSArray<NSString *> *windowStrings;
@property BOOL recheckNeeded;
@end

static BOOL ThaumistryWindowCheck(ZMenuDetection *menu, NSArray<ZMenuWindowText *> *windows);
static void BeyondZorkAfterMatch(ZMenuDetection *menu, NSUInteger patternIndex);
static void UnforgottenAfterMatch(ZMenuDetection *menu, NSUInteger patternIndex);

static ZMenuRule *Rule(kGameIdentity gameID, NSArray<NSString *> *formats, unichar initialChar, NSArray<NSString *> *patterns) {
    ZMenuRule *rule = [ZMenuRule new];
    rule.gameID = gameID;
    rule.formats = formats ? [NSSet setWithArray:formats] : nil;
    rule.initialChar = initialChar;
    rule.patterns = patterns;
    return rule;
}

static NSArray<ZMenuRule *> *Rules(void) {
    static NSArray<ZMenuRule *> *rules;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSArray *zcodeOrGlulx = @[@"zcode", @"glulx"];

        ZMenuRule *juniorArithmancer = Rule(kGameIsJuniorArithmancer, nil, '\0', nil);
        juniorArithmancer.noMenus = YES;

        // Thaumistry: Charm's Way has no instructions and uses bullets.
        ZMenuRule *thaumistry = Rule(kGameIsThaumistry, @[@"tads3"], u'•', nil);
        thaumistry.extraCheck = ThaumistryWindowCheck;

        ZMenuRule *beyondZork = Rule(kGameIsBeyondZork, zcodeOrGlulx, ' ', @[kBeyondZorkPattern, kBeyondZorkDefinitionsPattern, kBeyondZorkDecSystemPattern]);
        beyondZork.afterMatch = BeyondZorkAfterMatch;

        ZMenuRule *zcode = Rule(kGameIsGeneric, @[@"zcode"], ' ', @[kEqualsPattern, kUnforgottenPattern]);
        zcode.afterMatch = UnforgottenAfterMatch;

        rules = @[
            juniorArithmancer,
            thaumistry,
            // TADS 3: a cluster of lines that all start with a '>'.
            Rule(kGameIsGeneric, @[@"tads3"], '>', @[kTads3Pattern]),
            beyondZork,
            Rule(kGameIsAnchorheadOriginal, zcodeOrGlulx, ' ', @[kAnchorheadPattern]),
            Rule(kGameIsVespers, zcodeOrGlulx, ' ', @[kEqualsPattern, kVespersPattern]),
            zcode,
            Rule(kGameIsGeneric, @[@"glulx"], ' ', @[kEqualsPattern]),
            Rule(kGameIsGeneric, @[@"hugo"], ' ', @[kHugoPattern])
        ];
    });
    return rules;
}

static ZMenuRule * _Nullable RuleFor(NSString *format, kGameIdentity gameID) {
    for (ZMenuRule *rule in Rules()) {
        if (rule.gameID != kGameIsGeneric && rule.gameID != gameID)
            continue;
        if (rule.formats && ![rule.formats containsObject:format])
            continue;
        return rule;
    }
    return nil;
}

#pragma mark - ZMenuWindowText

@implementation ZMenuWindowText

- (instancetype)initWithText:(NSAttributedString *)text isGrid:(BOOL)isGrid frame:(NSRect)frame backgroundColor:(NSColor *)backgroundColor {
    self = [super init];
    if (self) {
        _text = text;
        _isGrid = isGrid;
        _frame = frame;
        _backgroundColor = backgroundColor;
    }
    return self;
}

@end

#pragma mark - ZMenuDetection

@implementation ZMenuDetection

- (NSArray<NSValue *> *)lines {
    return _menuLines;
}

- (NSString *)stringForLine:(NSUInteger)index {
    if (index >= _menuLines.count)
        return @"";
    NSRange range = NSIntersectionRange(NSMakeRange(0, _text.length), _menuLines[index].rangeValue);
    return [_text.string substringWithRange:range];
}

#pragma mark Clusters

// What we call a cluster is a number of adjacent lines (more than one).
// delimited by blank lines or the beginning or end of the text.

- (NSArray<NSValue *> *)largestClusterInWindows:(NSArray<ZMenuWindowText *> *)windows startingWithCharacter:(unichar)startChar {
    NSArray *menuLines = @[];
    NSMutableArray *windowStrings = [NSMutableArray new];
    ZMenuWindowText *windowWithCluster = nil;
    for (ZMenuWindowText *window in windows) {
        NSString *string = window.text.string;
        NSUInteger maxLength = window.isGrid ? kMaxGridTextLength : kMaxBufferTextLength;
        if (string.length > 0 && string.length < maxLength && window.frame.size.height > 0) {
            [windowStrings addObject:string];
            NSArray *cluster = [self largestClusterInString:string lines:string.lineRanges startingWithCharacter:startChar];
            if (cluster.count > menuLines.count) {
                menuLines = cluster;
                windowWithCluster = window;
            }
        }
    }
    _windowStrings = windowStrings;
    if (menuLines.count) {
        _text = windowWithCluster.text;
        _backgroundColor = windowWithCluster.backgroundColor;
    }
    return menuLines;
}

- (NSArray *)largestClusterInString:(NSString *)string lines:(NSArray<NSValue *> *)lines startingWithCharacter:(unichar)startChar {
    NSArray *menuLines = @[];
    NSUInteger line = 0;
    NSArray *currentCluster;
    NSArray *lastCluster = @[];
    do {
        currentCluster = [self clusterInString:string lines:lines startingWithCharacter:startChar atIndex:line];

        // Some games have a blank line as a divider in the middle of
        // the menu, so look for additional clusters and add these if found.
        if (currentCluster.count && lastCluster.count && [lines indexOfObject:lastCluster.lastObject] == line - 1 &&
            ABS(LeadingSpaces(string, [currentCluster.firstObject rangeValue], YES) -
                LeadingSpaces(string, [lastCluster.lastObject rangeValue], YES)) < 3 ) {
            if (!_isHugo) {
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

- (NSArray *)clusterInString:(NSString *)string lines:(NSArray<NSValue *> *)lines startingWithCharacter:(unichar)character atIndex:(NSUInteger)startPos {
    NSMutableArray *menulines = [[NSMutableArray alloc] init];
    if (!lines.count)
        return menulines;
    for (NSUInteger index = startPos; index < lines.count; index++) {
        // Check for leading char
        if (FirstCharacter(string, lines, index) != character &&
            FirstCharacter(string, lines, index - 1) != character &&
            FirstCharacter(string, lines, index + 1) != character) {
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
    NSString *string = _text.string;
    NSMutableArray<NSValue *> *lines = _menuLines.mutableCopy;

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

#pragma mark Instructions

// Finds the instructions in the first window that matches regexString, and
// if they are in the menu window, drops them and the lines above them from
// the menu.
- (BOOL)extractCommandsUsingPattern:(NSString *)regexString {
    NSMutableDictionary *menuDict = [[NSMutableDictionary alloc] init];
    NSMutableArray *keys = [[NSMutableArray alloc] init];
    NSRegularExpression *regex =
    [NSRegularExpression regularExpressionWithPattern:regexString
                                              options:0
                                                error:nil];
    for (NSString *string in _windowStrings) {
        NSArray *matches =
        [regex matchesInString:string
                       options:0
                         range:NSMakeRange(0, string.length)];
        for (NSTextCheckingResult *match in matches) {
            NSString *key = [string substringWithRange:[match rangeAtIndex:2]];
            NSString *value = [string substringWithRange:[match rangeAtIndex:1]];

            if (key.length) {
                if (!menuDict[key])
                    [keys addObject:key];
                menuDict[key] = value;
            }
        }
        if (matches.count) {
            if ([string isEqualToString:_text.string]) {
                // Remove the lines with instructions from the
                // detected menu cluster
                NSRange matchRange = ((NSTextCheckingResult *)matches.firstObject).range;
                matchRange = NSUnionRange(matchRange, ((NSTextCheckingResult *)matches.lastObject).range);
                NSUInteger lastOverlap = NSNotFound;
                for (NSUInteger i = 0; i < _menuLines.count; i++) {
                    NSRange range = _menuLines[i].rangeValue;
                    NSRange intersect = NSIntersectionRange(range, matchRange);
                    if (intersect.length)
                        lastOverlap = i;
                    if (NSMaxRange(range) > NSMaxRange(matchRange))
                        break;
                }
                if (lastOverlap < _menuLines.count - 1) {
                    _recheckNeeded = YES;
                    _menuLines.array = [_menuLines subarrayWithRange:NSMakeRange(lastOverlap + 1, _menuLines.count - lastOverlap - 1)];
                    while (_menuLines.count && [string rangeIsEmpty:_menuLines.firstObject])
                        [_menuLines removeObjectAtIndex:0];
                }
            }

            break;
        }
    }
    _commands = menuDict;
    _keys = keys;
    return menuDict.count > 0;
}

#pragma mark Selected line

- (NSUInteger)findSelectedLine {
    if (_menuLines.count == 1)
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

- (NSString *)trimmedLine:(NSUInteger)index {
    return [[self stringForLine:index] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

// The only line that starts with '>' or '*'.
- (NSUInteger)findLeadingSelectionCharacter {
    NSUInteger guess = NSNotFound;
    for (NSUInteger i = 0; i < _menuLines.count; i++) {
        NSString *line = [self trimmedLine:i];
        unichar firstChar = line.length ? [line characterAtIndex:0] : '\0';
        if (firstChar == '>' || firstChar == '*') {
            if (guess != NSNotFound)
                return NSNotFound;
            guess = i;
        }
    }
    return guess;
}

// The only line that ends with '*'.
- (NSUInteger)findTrailingSelectionCharacter {
    NSUInteger guess = NSNotFound;
    for (NSUInteger i = 0; i < _menuLines.count; i++) {
        NSString *line = [self trimmedLine:i];
        unichar lastChar = line.length ? [line characterAtIndex:line.length - 1] : '\0';
        if (lastChar == '*') {
            if (guess != NSNotFound)
                return NSNotFound;
            guess = i;
        }
    }
    return guess;
}

- (NSUInteger)findAttributeSelection {
    NSUInteger __block guess = NSNotFound;
    NSRange lineRange = _menuLines.firstObject.rangeValue;
    NSRange menuRange = NSUnionRange(lineRange, _menuLines.lastObject.rangeValue);
    NSRange allText = NSMakeRange(0, _text.length);
    menuRange = NSIntersectionRange(menuRange, allText);
    if (!_text.length)
        return NSNotFound;
    NSDictionary *attrs = [_text attributesAtIndex:0 effectiveRange:nil];
    NSColor *originalColor = attrs[NSBackgroundColorAttributeName];
    if (!originalColor)
        originalColor = _backgroundColor;
    NSArray<NSValue *> *lines = _menuLines;
    [_text
     enumerateAttribute:NSBackgroundColorAttributeName
     inRange:allText
     options:0
     usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (value && ![value isEqualToColor:originalColor]) {
            NSRange intersection = NSIntersectionRange(menuRange, range);
            if (intersection.length && intersection.length < menuRange.length) { // Attribute change in menu range
                for (NSUInteger i = 0; i < lines.count; i++)  {
                    NSRange thisLine = lines[i].rangeValue;
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
    if (guess != NSNotFound && _isTads3 && _menuLines.count == 2) {
        guess = (guess == 0) ? 1 : 0;
    }

    // If no match, look for a unique foreground color
    // Does not work with less than 3 items except in TADS 3
    if (guess == NSNotFound && (_menuLines.count >= 3 || _isTads3)) {
        NSMutableArray *foregroundColors = [[NSMutableArray alloc] initWithCapacity:_menuLines.count];
        NSMutableArray *backgroundColors = [[NSMutableArray alloc] initWithCapacity:_menuLines.count];

        for (NSUInteger i = 0; i < _menuLines.count; i++) {
            NSRange range = _menuLines[i].rangeValue;
            NSUInteger spaces = (NSUInteger)LeadingSpaces(_text.string, range, NO);
            range = NSMakeRange(range.location + spaces, range.length - spaces);
            range = NSIntersectionRange(allText, range);
            if (range.location >= _text.length) {
                [foregroundColors addObject:[NSNull null]];
                [backgroundColors addObject:[NSNull null]];
                continue;
            }
            NSDictionary *attrDict = [_text attributesAtIndex:range.location effectiveRange:nil];

            NSColor *lineForeground = attrDict[NSForegroundColorAttributeName];
            [foregroundColors addObject:lineForeground ?: [NSNull null]];

            NSColor *lineBackground = attrDict[NSBackgroundColorAttributeName];
            [backgroundColors addObject:lineBackground ?: [NSNull null]];

            // TADS 3 menus lines usually begin with a ">" which is made invisible in non-selected lines
            // by giving the text and background the same color.
            // In a two-line TADS 3 menu, we assume that the line where foreground and background don't match
            // (and thus the ">" is visible) is the selected line.
            if (_isTads3 && _menuLines.count == 2 && lineBackground != nil && ![lineBackground isEqual:lineForeground]) {
                return i;
            }
        }

        guess = UniqueColorIndex(foregroundColors);

        // If no match, look for a unique background color
        if (guess == NSNotFound)
            guess = UniqueColorIndex(backgroundColors);
    }

    return guess;
}

@end

#pragma mark - Per-game hooks

// Two buffer windows, one with only one line, and a hidden one.
static BOOL ThaumistryWindowCheck(ZMenuDetection *menu, NSArray<ZMenuWindowText *> *windows) {
    if (windows.count != 3)
        return NO;
    ZMenuWindowText *hidden = nil;
    ZMenuWindowText *title = nil;
    ZMenuWindowText *menuWindow = nil;
    for (ZMenuWindowText *window in windows) {
        if (!window.text)
            continue;
        if (window.text == menu.text) {
            menuWindow = window;
        } else if (window.frame.size.height == 0) {
            hidden = window;
        } else {
            NSString *string = window.text.string;
            NSRange allText = NSMakeRange(0, string.length);
            NSRange firstLine = [string lineRangeForRange:NSMakeRange(0, 0)];
            if (NSEqualRanges(allText, firstLine))
                title = window;
        }
    }
    return (title != nil && menuWindow != nil && hidden != nil);
}

// The Function Key Definitions menu has no instructions, and its last two
// lines are set apart from the rest.
static void BeyondZorkAfterMatch(ZMenuDetection *menu, NSUInteger patternIndex) {
    if (patternIndex != 1)
        return;
    menu.commands = @{};
    menu.keys = @[];
    NSString *string = menu.text.string;
    NSRange range = [string rangeOfString:@"Restore Defaults"];
    if (range.length) {
        [menu.menuLines addObject:[NSValue valueWithRange:range]];
        range = [string rangeOfString:@"Exit"];
        if (range.length) {
            [menu.menuLines addObject:[NSValue valueWithRange:range]];
        }
    }
}

// Skip the empty fourth line in The Unforgotten.
static void UnforgottenAfterMatch(ZMenuDetection *menu, NSUInteger patternIndex) {
    if (patternIndex == 1 && menu.menuLines.count > 3)
        [menu.menuLines removeObjectAtIndex:3];
}

#pragma mark - ZMenuDetector

@implementation ZMenuDetector

+ (ZMenuDetection *)detectMenuInWindows:(NSArray<ZMenuWindowText *> *)windows format:(NSString *)format gameID:(kGameIdentity)gameID {
    ZMenuRule *rule = RuleFor(format, gameID);
    if (!rule || rule.noMenus)
        return nil;

    ZMenuDetection *menu = [ZMenuDetection new];
    menu.isTads3 = [format isEqualToString:@"tads3"];
    menu.isHugo = [format isEqualToString:@"hugo"];
    menu.isBeyondZork = (gameID == kGameIsBeyondZork);
    menu.commands = @{};
    menu.keys = @[];

    menu.menuLines = [[menu largestClusterInWindows:windows startingWithCharacter:rule.initialChar] mutableCopy];
    if (!menu.menuLines.count)
        return nil;

    // Check if the number of lines are within bounds
    if (menu.menuLines.count > kMaxMenuLines) {
        NSLog(@"Too many lines (%lu). Not in a menu", menu.menuLines.count);
        return nil;
    }

    if (rule.extraCheck && !rule.extraCheck(menu, windows))
        return nil;

    if (rule.patterns) {
        NSUInteger matched = NSNotFound;
        for (NSUInteger i = 0; i < rule.patterns.count; i++) {
            if ([menu extractCommandsUsingPattern:rule.patterns[i]]) {
                matched = i;
                break;
            }
        }
        // If we didn't find any instructions, decide this is not a menu
        if (matched == NSNotFound)
            return nil;
        if (rule.afterMatch)
            rule.afterMatch(menu, matched);

        if (menu.recheckNeeded) {
            menu.menuLines.array = [menu recheckClusterStartingWithCharacter:rule.initialChar];
            if (!menu.menuLines.count) {
                //Failed recheck. Give up.
                return nil;
            }
        }
    }

    // Now we check if the initial line is actually not part of the menu,
    // as seen in Vespers.
    // We assume this to be the case if the first has no leading spaces
    // but the next line has.
    NSString *string = menu.text.string;
    if (menu.menuLines.count > 2 && !LeadingSpaces(string, menu.menuLines[0].rangeValue, YES) &&
        LeadingSpaces(string, menu.menuLines[1].rangeValue, YES)) {
        [menu.menuLines removeObjectAtIndex:0];
    }

    // Otherwise, look for >, *, •, change of color or reverse video
    menu.selectedLine = [menu findSelectedLine];

    // If we find no currently selected line, decide this is not a menu
    if (menu.selectedLine == NSNotFound)
        return nil;

    return menu;
}

+ (NSString *)titleForMenu:(ZMenuDetection *)menu inWindows:(NSArray<ZMenuWindowText *> *)windows {
    ZMenuWindowText *top = nil;
    CGFloat topPos = CGFLOAT_MAX;
    // Find upper window. The game view is flipped, so it has the smallest y.
    for (ZMenuWindowText *window in windows) {
        if (window.text.length && window.frame.size.height > 0 && window.frame.origin.y < topPos) {
            top = window;
            topPos = window.frame.origin.y;
        }
    }
    NSString *string = [top.text.string componentsSeparatedByString:@"\n"].firstObject;

    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if ([string rangeOfString:@"    "].location != NSNotFound)
        string = @"";
    if (menu.isBeyondZork && string.length == 0) {
        NSString *topString = [[menu stringForLine:0] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([topString isEqualToString:@"Begin using a preset character"])
            string = @"Character Setup";
        else if ([topString isEqualToString:@"F1"])
            string = @"Function Key Definitions";
    }
    return string ?: @"";
}

+ (NSString *)instructionsForMenu:(ZMenuDetection *)menu {
    NSString *string = @"";
    if (menu.isBeyondZork) {
        string = menu.commands[menu.keys.firstObject];
        if (!string.length)
            return @"";
        string = [string stringByReplacingOccurrencesOfString:@"↑" withString:@"UP ARROW"];
        string = [string stringByReplacingOccurrencesOfString:@"↓" withString:@"DOWN ARROW"];
        string = [string stringByReplacingOccurrencesOfString:@"←" withString:@"LEFT ARROW"];
        string = [string stringByReplacingOccurrencesOfString:@"→" withString:@"RIGHT ARROW"];
        return [string stringByAppendingString:@"\n"];
    }
    for (NSString *key in menu.keys) {
        // Remove brackets in result
        // First the entire word "Enter] "
        NSString *valueString = menu.commands[key];
        valueString =
        [BracketedWordRegex() stringByReplacingMatchesInString:valueString
                                                       options:0
                                                         range:NSMakeRange(0, valueString.length)
                                                  withTemplate:@""];
        //Then any brackets in things like "N]ext"
        valueString = [valueString stringByReplacingOccurrencesOfString:@"]" withString:@""];

        valueString = valueString.capitalizedString;

        string = [string stringByAppendingFormat:@"%@: \"%@\".\n", valueString, key];
    }
    return string;
}

@end

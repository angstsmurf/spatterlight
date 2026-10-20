//
//  ZMenu.m
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import "ZMenu.h"
#import "ZMenuDetector.h"

#import "GlkController.h"
#import "GlkController+Speech.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "Game.h"
#import "Theme.h"
#import "InfocomV6MenuHandler.h"

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

@interface ZMenu ()

// The menu found by the last successful detectMenu.
@property ZMenuDetection *detection;
@property NSUInteger selectedLine;
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

// The game's windows as the detector sees them.
- (NSArray<ZMenuWindowText *> *)windowTexts {
    NSMutableArray<ZMenuWindowText *> *windows = [NSMutableArray new];
    for (GlkWindow *view in _glkctl.gwindows.allValues) {
        NSTextView *textview = TextViewOf(view);
        [windows addObject:[[ZMenuWindowText alloc] initWithText:textview.textStorage
                                                          isGrid:[view isKindOfClass:[GlkTextGridWindow class]]
                                                           frame:view.frame
                                                 backgroundColor:textview.backgroundColor]];
    }
    return windows;
}

- (BOOL)detectMenu {
    GlkController *glkctl = _glkctl;
    NSString *format = glkctl.game.detectedFormat;
    if (glkctl.showingInfocomV6Menu && ([format isEqualToString:@"glulx"] || [format isEqualToString:@"zcode"] || [format isEqualToString:@"hugo"]))
        return YES;

    ZMenuDetection *detection = [ZMenuDetector detectMenuInWindows:[self windowTexts] format:format gameID:glkctl.gameID];
    if (!detection)
        return NO;
    _detection = detection;
    _selectedLine = detection.selectedLine;
    return YES;
}

- (NSUInteger)findSelectedLine {
    if (_glkctl.showingInfocomV6Menu) {
        return _glkctl.infocomV6MenuHandler.selectedLine;
    }
    return [_detection findSelectedLine];
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
    if (_lastNumberOfItems && _lastNumberOfItems != _detection.lines.count && !_glkctl.showingInfocomV6Menu)
        _haveSpokenMenu = NO;
    _lastNumberOfItems = _detection.lines.count;

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
        NSString *titleString = [ZMenuDetector titleForMenu:_detection inWindows:[self windowTexts]];
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

    NSString *menuItemString = [_detection stringForLine:_selectedLine];

    menuItemString = [SelectionMarkerRegex() stringByReplacingMatchesInString:menuItemString
                                                         options:0
                                                           range:NSMakeRange(0, menuItemString.length)
                                                    withTemplate:@""];
    NSString *trimmedString = [menuItemString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmedString.length == 0)
        menuItemString = @"Empty line.";

    // Add pre-loaded input field text to Beyond Zork definitions menu
    if (_glkctl.gameID == kGameIsBeyondZork) {
        id delegate = ((NSTextStorage *)_detection.text).delegate;
        if ([delegate isKindOfClass:[GlkTextGridWindow class]] && ((GlkTextGridWindow *)delegate).input) {
            menuItemString = [menuItemString stringByAppendingString:((GlkTextGridWindow *)delegate).enteredTextSoFar];
        }
    }

    if (useIndex) {
        NSString *indexString = [NSString stringWithFormat:@".\nMenu item %lu", _selectedLine + 1];
        if (useTotal) {
            indexString = [indexString stringByAppendingString:
                           [NSString stringWithFormat:@" of %lu", _detection.lines.count]];
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
    return [ZMenuDetector instructionsForMenu:_detection];
}

@end

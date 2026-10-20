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
        NSTextView *textview = view.textview;
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

// The menu to speak: an Infocom V6 menu reported by the interpreter, if one
// is showing, or else the text menu found by detectMenu.
- (id<VoiceOverMenu>)currentMenu {
    if (_glkctl.showingInfocomV6Menu)
        return _glkctl.infocomV6MenuHandler;
    return self;
}

#pragma mark Speech

- (void)cancelPendingSpeech {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)speakSelectedLine {
    if (!self.currentMenu.haveSpokenMenu) {
        [self performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:kFirstSpeakDelay];
    } else {
        [self performSelector:@selector(deferredSpeakSelectedLine:) withObject:nil afterDelay:0];
    }
}

-(void)deferredSpeakSelectedLine:(id)sender {
    id<VoiceOverMenu> menu = self.currentMenu;
    if ([menu findSelectedLine] == NSNotFound)
        return;
    [self cancelPendingSpeech];

    // We use the number of menu lines as a proxy to see if we have just switched to a new menu
    if (menu == self) {
        if (_lastNumberOfItems && _lastNumberOfItems != _detection.lines.count)
            _haveSpokenMenu = NO;
        _lastNumberOfItems = _detection.lines.count;
    }

    if (!menu.haveSpokenMenu) {
        menu.haveSpokenMenu = YES;
        [self speakString:[menu menuLineStringWithTitle:YES index:YES total:YES instructions:YES]];
        return;
    }

    GlkController *glkctl = _glkctl;
    NSString *selectedLineString = [menu menuLineStringWithTitle:NO index:(glkctl.theme.vOSpeakMenu >= kVOMenuIndex) total:(glkctl.theme.vOSpeakMenu == kVOMenuTotal) instructions:NO];

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
    [self speakString:selectedLineString];
}

- (void)speakInstructions:(id)sender {
    [_glkctl announce:[self.currentMenu instructionString] priority:NSAccessibilityPriorityLow];
}

- (void)speakString:(NSString *)string {
    if (!string || string.length == 0)
        return;
    [self cancelPendingSpeech];
    if (_glkctl.gameID == kGameIsBeyondZork) {
        // Delete graphic indicator
        string = [BeyondZorkIndicatorRegex() stringByReplacingMatchesInString:string
                                                     options:0
                                                       range:NSMakeRange(0, string.length)
                                                withTemplate:@""];

    }

    [_glkctl speakStringNow:string];
}

#pragma mark VoiceOverMenu

- (NSUInteger)findSelectedLine {
    _selectedLine = [_detection findSelectedLine];
    return _selectedLine;
}

- (NSString *)menuLineStringWithTitle:(BOOL)useTitle index:(BOOL)useIndex total:(BOOL)useTotal instructions:(BOOL)useInstructions {
    NSString *menuItemString = [_detection stringForLine:_selectedLine] ?: @"";

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
        menuItemString = [menuItemString stringByAppendingString:VoiceOverMenuIndexString(@"Menu item", _selectedLine, _detection.lines.count, useTotal)];
    }

    if (useInstructions) {
        menuItemString = [menuItemString stringByAppendingString:[self instructionString]];
    }

    if (useTitle) {
        NSString *title = [ZMenuDetector titleForMenu:_detection inWindows:[self windowTexts]];
        menuItemString = [VoiceOverMenuTitleString(@"We are in a menu", title) stringByAppendingString:menuItemString];
    }

    return menuItemString;
}

- (NSString *)instructionString {
    return [ZMenuDetector instructionsForMenu:_detection];
}

@end

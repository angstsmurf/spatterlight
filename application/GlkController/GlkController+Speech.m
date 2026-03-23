#import "GlkController_Private.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "GlkGraphicsWindow.h"
#import "Theme.h"
#import "Game.h"
#import "Metadata.h"
#import "Preferences.h"
#import "RotorHandler.h"
#import "ZMenu.h"
#import "BureaucracyForm.h"
#import "JourneyMenuHandler.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#else
#define NSLog(...)
#endif

@implementation GlkController (Speech)

- (BOOL)isAccessibilityElement {
    return NO;
}

- (NSArray *)accessibilityCustomActions {
    NSAccessibilityCustomAction *speakMostRecent = [[NSAccessibilityCustomAction alloc]
                                                    initWithName:NSLocalizedString(@"repeat the text output of the last move", nil) target:self selector:@selector(speakMostRecent:)];
    NSAccessibilityCustomAction *speakPrevious = [[NSAccessibilityCustomAction alloc]
                                                  initWithName:NSLocalizedString(@"step backward through moves", nil) target:self selector:@selector(speakPrevious:)];
    NSAccessibilityCustomAction *speakNext = [[NSAccessibilityCustomAction alloc]
                                              initWithName:NSLocalizedString(@"step forward through moves", nil) target:self selector:@selector(speakNext:)];
    NSAccessibilityCustomAction *speakStatus = [[NSAccessibilityCustomAction alloc]
                                                initWithName:NSLocalizedString(@"speak status bar text", nil) target:self selector:@selector(speakStatus:)];

    return @[speakStatus, speakNext, speakPrevious, speakMostRecent];
}


- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {

    if ([keyPath isEqualToString:@"voiceOverEnabled"]) {
        self.voiceOverActive = [NSWorkspace sharedWorkspace].voiceOverEnabled;
        if (self.voiceOverActive) { // VoiceOver was switched on
            // Don't speak or change menus unless we are the top game
            if ([Preferences.instance currentGame] == self.game && !dead) {
                [self.journeyMenuHandler showJourneyMenus];
                if (self.journeyMenuHandler.shouldShowDialog) {
                    [self.journeyMenuHandler recreateDialog];
                } else {
                    [self speakOnBecomingKey];
                }
            }
        } else { // VoiceOver was switched off
            [self.journeyMenuHandler hideJourneyMenus];
        }
        // We send an event to make the interpreter aware of the new VoiceOver status.
        [self sendArrangeEventWithFrame:self.gameView.frame force:YES];
    } else {
        // Any unrecognized context must belong to super
        [super observeValueForKeyPath:keyPath
                             ofObject:object
                               change:change
                              context:context];
    }
}

- (IBAction)saveAsRTF:(id)sender {
    GlkWindow *largest = self.largestWithMoves;
    if (largest && [largest isKindOfClass:[GlkTextBufferWindow class]] ) {
        [largest saveAsRTF:self];
        return;
    }
    largest = nil;
    NSUInteger longestTextLength = 0;
    for (GlkWindow *win in self.gwindows.allValues) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]]) {
            GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)win;
            NSUInteger length = bufWin.textview.string.length;
            if (length > longestTextLength) {
                longestTextLength = length;
                largest = win;
            }
        }
    }

    if (largest) {
        [largest saveAsRTF:self];
        return;
    }
}


#pragma mark ZMenu

- (void)checkZMenuAndSpeak:(BOOL)speak {
    if (!self.voiceOverActive || self.mustBeQuiet || self.theme.vOSpeakMenu == kVOMenuNone) {
        return;
    }
    if (self.shouldCheckForMenu) {
        self.shouldCheckForMenu = NO;
        if (!self.zmenu) {
            self.zmenu = [[ZMenu alloc] initWithGlkController:self];
        }
        if (![self.zmenu isMenu]) {
            [NSObject cancelPreviousPerformRequestsWithTarget:self.zmenu];
            self.zmenu.glkctl = nil;
            self.zmenu = nil;
        }

        // Bureaucracy form accessibility
        if (!self.zmenu && self.gameID == kGameIsBureaucracy) {
            if (!self.form) {
                self.form = [[BureaucracyForm alloc] initWithGlkController:self];
            }
            if (![self.form isForm]) {
                [NSObject cancelPreviousPerformRequestsWithTarget:self.form];
                self.form.glkctl = nil;
                self.form = nil;
            } else if (speak) {
                [self.form speakCurrentField];
            }
        }
    }
    if (self.zmenu) {
        for (GlkTextBufferWindow *view in self.gwindows.allValues) {
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                [view setLastMove];
        }
        if (speak)
            [self.zmenu speakSelectedLine];
    }
}

- (BOOL)showingInfocomV6Menu {
    return (_infocomV6MenuHandler != nil);
}

#pragma mark Speak new text

- (void)speakNewText {
    // Find a "main text window"
    NSMutableArray *windowsWithText = self.gwindows.allValues.mutableCopy;
    NSMutableArray *bufWinsWithoutText = [[NSMutableArray alloc] init];
    for (GlkWindow *view in self.gwindows.allValues) {
        if ([view isKindOfClass:[GlkGraphicsWindow class]] || ![(GlkTextBufferWindow *)view setLastMove]) {
            // Remove all Glk window objects with no new text to speak
            [windowsWithText removeObject:view];
            if ([view isKindOfClass:[GlkTextBufferWindow class]] && ((GlkTextBufferWindow *)view).moveRanges.count) {
                [bufWinsWithoutText addObject:view];
            }
        }
    }

    if (self.quoteBoxes.count)
        [windowsWithText addObject:self.quoteBoxes.lastObject];

    if (!windowsWithText.count) {
        return;
    } else {
        NSMutableArray *bufWinsWithText = [[NSMutableArray alloc] init];
        for (GlkWindow *view in windowsWithText)
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                [bufWinsWithText addObject:view];
        if (bufWinsWithText.count == 1) {
            [self speakLargest:bufWinsWithText];
            return;
        } else if (bufWinsWithText.count == 0 && bufWinsWithoutText.count > 0) {
            [self speakLargest:bufWinsWithoutText];
            return;
        }
    }
    [self speakLargest:windowsWithText];
}

- (void)speakLargest:(NSArray *)array {
    if (self.mustBeQuiet || self.zmenu || self.form || !self.voiceOverActive) {
        return;
    }

    GlkWindow *largest = nil;
    if (array.count == 1) {
        largest = (GlkWindow *)array.firstObject;
    } else {
        CGFloat largestSize = 0;
        for (GlkWindow *view in array) {
            CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
            if (size > largestSize) {
                largestSize = size;
                largest = view;
            }
        }
    }
    if (largest) {
        if ([largest isKindOfClass:[GlkTextGridWindow class]] && self.quoteBoxes.count) {
            GlkTextGridWindow *box = self.quoteBoxes.lastObject;

            NSString *str = box.textview.string;
            if (str.length) {
                str = [@"QUOTE: \n\n" stringByAppendingString:str];
                [self speakString:str];
                return;
            }
        }
        [largest repeatLastMove:self];
    }
}

- (void)speakOnBecomingKey {
    if (self.voiceOverActive) {
        self.shouldCheckForMenu = YES;
        [self checkZMenuAndSpeak:NO];
        if (self.theme.vODelayOn && !self.mustBeQuiet) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [self speakMostRecentAfterDelay];
            });
        }
    } else {
        self.zmenu = nil;
        self.form = nil;
    }
}

#pragma mark Speak previous moves

// If sender == self, never announce "No last move to speak!"
- (IBAction)speakMostRecent:(id)sender {
    if (self.zmenu) {
        NSString *menuString = [self.zmenu menuLineStringWithTitle:YES index:YES total:YES instructions:YES];
        self.zmenu.haveSpokenMenu = YES;
        [self speakString:menuString];
        return;
    } else if (self.form) {
        NSString *formString = [self.form fieldStringWithTitle:YES andIndex:YES andTotal:YES];
        self.form.haveSpokenForm = YES;
        [self speakString:formString];
        return;
    }
    GlkWindow *mainWindow = self.largestWithMoves;
    if (!mainWindow) {
        if (sender != self)
            [self speakStringNow:@"No last move to speak!"];
        return;
    }

    if (self.quoteBoxes.count) {
        self.speechTimeStamp = [NSDate distantPast];
    }
    [mainWindow setLastMove];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [mainWindow repeatLastMove:sender];
    });
}

- (void)speakMostRecentAfterDelay {
    for (GlkWindow *win in self.gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextBufferWindow class]])
            [(GlkTextBufferWindow *)win resetLastSpokenString];
    }

    CGFloat delay = self.theme.vOHackDelay;
    shouldAddTitlePrefixToSpeech = (delay < 1);
    delay *= NSEC_PER_SEC;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)delay), dispatch_get_main_queue(), ^(void) {
        [self speakMostRecent:self];
    });
}

- (IBAction)speakPrevious:(id)sender {
    GlkWindow *mainWindow = self.largestWithMoves;
    if (!mainWindow) {
        [self speakStringNow:@"No previous move to speak!"];
        return;
    }
    [mainWindow speakPrevious];
}

- (IBAction)speakNext:(id)sender {
    GlkWindow *mainWindow = self.largestWithMoves;
    if (!mainWindow) {
        [self speakStringNow:@"No next move to speak!"];
        return;
    }
    [mainWindow speakNext];
}

- (IBAction)speakStatus:(id)sender {
    GlkWindow *win;

    // Lazy heuristic to find Tads 3 status window: if there are more than one window and
    // only one of them sits at the top, pick that one
    if ( self.gwindows.allValues.count > 1 && [self.game.detectedFormat isEqualToString:@"tads3"]) {
        NSMutableArray<GlkWindow *> *array = [[NSMutableArray alloc] initWithCapacity: self.gwindows.allValues.count];
        for (win in self.gwindows.allValues) {
            if (win.frame.origin.y == 0 && ![win isKindOfClass:[GlkGraphicsWindow class]]) {
                [array addObject:win];
            }
        }
        if (array.count == 1) {
            [array.firstObject speakStatus];
            return;
        }
    }

    // Try to find status window to pass this on to
    for (win in self.gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextGridWindow class]]) {
            [(GlkTextGridWindow *)win speakStatus];
            return;
        }
    }
    [self speakStringNow:@"No status window found!"];
}

- (void)forceSpeech {
    self.lastSpokenString = nil;
    self.speechTimeStamp = [NSDate distantPast];
}

- (void)speakStringNow:(NSString *)string {
    [self forceSpeech];
    [self speakString:string];
}

- (void)speakString:(NSString *)string {
    NSString *newString = string;

    if (string.length == 0 || !self.voiceOverActive)
        return;

    if ([string isEqualToString: self.lastSpokenString] && self.speechTimeStamp.timeIntervalSinceNow > -3.0) {
        return;
    }

    self.speechTimeStamp = [NSDate date];
    self.lastSpokenString = string;

    NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:@"\u00A0 >\n_\0\uFFFC"];
    newString = [newString stringByTrimmingCharactersInSet:charset];

    unichar nc = '\0';
    NSString *nullChar = [NSString stringWithCharacters:&nc length:1];
    newString = [newString stringByReplacingOccurrencesOfString:nullChar withString:@""];

    if (newString.length == 0)
        newString = string;

    if (shouldAddTitlePrefixToSpeech) {
        newString = [NSString stringWithFormat:@"Now in, %@: %@", [self gameTitle], newString];
        shouldAddTitlePrefixToSpeech = NO;
    }

    NSWindow *window = self.window;

    if (self.gameID == kGameIsJourney && Preferences.instance.currentGame == self.game) {
        window = NSApp.mainWindow;
    }

    NSAccessibilityPostNotificationWithUserInfo(
                                                window,
                                                NSAccessibilityAnnouncementRequestedNotification,
                                                @{NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
                                                  NSAccessibilityAnnouncementKey: newString
                                                });
}

- (GlkWindow *)largestWithMoves {
    // Find a "main text window"
    GlkWindow *largest = nil;

// This somewhat reduces VoiceOver confusingly speaking of the command area grid in Journey
    if (self.gameID == kGameIsJourney) {
        for (GlkWindow *view in self.gwindows.allValues) {
            if ([view isKindOfClass:[GlkTextBufferWindow class]])
                return view;
        }
    }

    NSMutableArray *windowsWithMoves = self.gwindows.allValues.mutableCopy;
    for (GlkWindow *view in self.gwindows.allValues) {
        // Remove all Glk windows without text from array
        if (!view.moveRanges || !view.moveRanges.count) {
            // An empty window with an attached quotebox is still considered to have text
            if (!(self.quoteBoxes && ([view isKindOfClass:[GlkTextBufferWindow class]] && ((GlkTextBufferWindow *)view).quoteBox)))
                [windowsWithMoves removeObject:view];
        }
    }

    if (!windowsWithMoves.count) {
        NSMutableArray *allWindows = self.gwindows.allValues.mutableCopy;
        if (self.quoteBoxes.count)
            [allWindows addObject:self.quoteBoxes.lastObject];
        for (GlkWindow *view in allWindows) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]] && ((GlkTextGridWindow *)view).textview.string.length > 0)
                [windowsWithMoves addObject:view];
        }
        if (!windowsWithMoves.count) {
            // No windows with text
            return nil;
        }
    }

    CGFloat largestSize = 0;
    for (GlkWindow *view in windowsWithMoves) {
        CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
        if (size > largestSize) {
            largestSize = size;
            largest = view;
        }
    }
    return largest;
}

#pragma mark Custom rotors

- (NSArray *)createCustomRotors {
    if (!self.rotorHandler) {
        self.rotorHandler = [RotorHandler new];
        self.rotorHandler.glkctl = self;
    }
    return self.rotorHandler.createCustomRotors;
}

- (NSString *)gameTitle {
    return self.game.metadata.title;
}

@end

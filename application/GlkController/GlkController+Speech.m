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
#import "SPAccessibility.h"

#import "DebugLog.h"

// The window with the largest frame, or nil if none has any area.
static GlkWindow *largestByFrameArea(NSArray<GlkWindow *> *windows) {
    GlkWindow *largest = nil;
    CGFloat largestSize = 0;
    for (GlkWindow *view in windows) {
        CGFloat size = fabs(view.frame.size.width * view.frame.size.height);
        if (size > largestSize) {
            largestSize = size;
            largest = view;
        }
    }
    return largest;
}

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
                [self.journeyMenuHandler showMenus];
                if (self.journeyMenuHandler.hasPendingDialog) {
                    [self.journeyMenuHandler recreateDialog];
                } else {
                    [self speakOnBecomingKey];
                }
            }
        } else { // VoiceOver was switched off
            [self.journeyMenuHandler hideMenus];
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
        NSUInteger length = [self speakableStringForWindow:win].length;
        if (length > longestTextLength) {
            longestTextLength = length;
            largest = win;
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
        if (![self.zmenu detectMenu]) {
            VoiceOverHelperDetach(self.zmenu);
            self.zmenu = nil;
        }

        // Bureaucracy form accessibility
        if (!self.zmenu && self.gameID == kGameIsBureaucracy) {
            if (!self.form) {
                self.form = [[BureaucracyForm alloc] initWithGlkController:self];
            }
            if (![self.form detectForm]) {
                VoiceOverHelperDetach(self.form);
                self.form = nil;
            } else {
                [self.form addCurrentFieldToMoves];
                if (speak)
                    [self.form speakCurrentField];
            }
        }
    }
    if (self.zmenu) {
        for (GlkTextBufferWindow *view in self.bufferWindows)
            [view setLastMove];
        if (speak)
            [self.zmenu speakSelectedLine];
    }
}

- (BOOL)showingInfocomV6Menu {
    return (_infocomV6MenuHandler != nil);
}

- (id<VoiceOverMenu>)currentMenu {
    if (!self.zmenu)
        return nil;
    if (self.showingInfocomV6Menu)
        return _infocomV6MenuHandler;
    return self.zmenu;
}

#pragma mark Speak new text

// How long (seconds) the UI must be free of layout churn before we let a
// VoiceOver announcement fire. A full-screen graphic dismissal triggers a
// border/status redraw (several window resizes) right after the new text is
// printed; each frame change makes AppKit re-read the focused window, which
// interrupts the announcement. Waiting for the churn to settle avoids that.
static const NSTimeInterval kLayoutSettleInterval = 0.45;

// (Re)schedule the new-text announcement. Called from flushDisplay; cancels any
// previously scheduled announcement so repeated flushes coalesce into one. The
// initial delay is computed by the caller (which has the journey-dialog ivar).
- (void)scheduleSpeakNewTextAfterDelay:(CGFloat)delay {
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(fireSpeakNewText) object:nil];
    [self performSelector:@selector(fireSpeakNewText) withObject:nil afterDelay:delay];
}

// Fire the announcement, but if the UI is still churning (a window resized
// within the last kLayoutSettleInterval) keep deferring until it settles, so
// VoiceOver isn't interrupted mid-sentence.
- (void)fireSpeakNewText {
    NSTimeInterval sinceChurn = NSProcessInfo.processInfo.systemUptime - self.lastLayoutChurnTimestamp;
    if (sinceChurn < kLayoutSettleInterval) {
        [self performSelector:@selector(fireSpeakNewText) withObject:nil afterDelay:kLayoutSettleInterval - sinceChurn + 0.02];
        return;
    }
    [self forceSpeech];
    [self speakNewText];
}

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

    GlkWindow *largest = (array.count == 1) ? array.firstObject : largestByFrameArea(array);
    if (largest) {
        if ([largest isKindOfClass:[GlkTextGridWindow class]] && self.quoteBoxes.count) {
            NSString *str = self.quoteBoxes.lastObject.speakableString;
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
            runOnMainAfterDelay(0.5, ^{
                [self speakMostRecentAfterDelay];
            });
        }
    } else {
        VoiceOverHelperDetach(self.zmenu);
        self.zmenu = nil;
        VoiceOverHelperDetach(self.form);
        self.form = nil;
    }
}

#pragma mark Speak previous moves

// If sender == self, never announce "No last move to speak!"
- (IBAction)speakMostRecent:(id)sender {
    id<VoiceOverMenu> menu = self.currentMenu;
    if (menu) {
        NSString *menuString = [menu menuLineStringWithTitle:YES index:YES total:YES instructions:YES];
        menu.haveSpokenMenu = YES;
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
    runOnMainAfterDelay(1.1, ^{
        [mainWindow repeatLastMove:sender];
    });
}

- (void)speakMostRecentAfterDelay {
    for (GlkTextBufferWindow *win in self.bufferWindows)
        [win resetLastSpokenString];

    CGFloat delay = self.theme.vOHackDelay;
    shouldAddTitlePrefixToSpeech = (delay < 1);
    runOnMainAfterDelay(delay, ^{
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
    // The grid status path normally cancels any pending menu/form speech; do the
    // same here since we no longer route through -[GlkTextGridWindow speakStatus].
    [self.zmenu cancelPendingSpeech];
    [self.form cancelPendingSpeech];

    // The main text/input window is read by the "speak move" actions, not here.
    GlkWindow *mainWindow = self.largestWithMoves;

    // Speak the status bar first, then every other non-graphic window that holds
    // text (skipping the main window and the status window already spoken), all
    // in a single announcement.
    GlkWindow *statusWindow = self.statusWindow;

    NSMutableArray<NSString *> *parts = [[NSMutableArray alloc] init];

    NSString *statusText = [self speakableStringForWindow:statusWindow];
    if (statusText.length)
        [parts addObject:statusText];

    // Read the remaining text windows in natural top-to-bottom, left-to-right
    // order so the speech follows the on-screen layout.
    NSArray<GlkWindow *> *others = [self.gwindows.allValues sortedArrayUsingComparator:^NSComparisonResult(GlkWindow *a, GlkWindow *b) {
        if (a.frame.origin.y != b.frame.origin.y)
            return a.frame.origin.y < b.frame.origin.y ? NSOrderedAscending : NSOrderedDescending;
        if (a.frame.origin.x != b.frame.origin.x)
            return a.frame.origin.x < b.frame.origin.x ? NSOrderedAscending : NSOrderedDescending;
        return NSOrderedSame;
    }];

    for (GlkWindow *win in others) {
        if (win == mainWindow || win == statusWindow)
            continue;
        NSString *text = [self speakableStringForWindow:win];
        if (text.length)
            [parts addObject:text];
    }

    if (!parts.count) {
        [self speakStringNow:@"No status window found!"];
        return;
    }

    [self speakStringNow:[parts componentsJoinedByString:@"\n"]];
}

// Find the window acting as the status bar, or nil if there is none.
- (nullable GlkWindow *)statusWindow {
    // Lazy heuristic to find a Tads 3 status window: if there is more than one
    // window and only one of them sits at the top, pick that one.
    if (self.gwindows.allValues.count > 1 && [self.game.detectedFormat isEqualToString:@"tads3"]) {
        NSMutableArray<GlkWindow *> *array = [[NSMutableArray alloc] initWithCapacity:self.gwindows.allValues.count];
        for (GlkWindow *win in self.gwindows.allValues) {
            if (win.frame.origin.y == 0 && ![win isKindOfClass:[GlkGraphicsWindow class]]) {
                [array addObject:win];
            }
        }
        if (array.count == 1)
            return array.firstObject;
    }

    // Otherwise the first text grid window is the status line.
    for (GlkWindow *win in self.gwindows.allValues) {
        if ([win isKindOfClass:[GlkTextGridWindow class]])
            return win;
    }
    return nil;
}

// The text a window contributes to the spoken status, or nil for windows with
// no text (e.g. graphics windows).
- (nullable NSString *)speakableStringForWindow:(nullable GlkWindow *)win {
    if ([win isKindOfClass:[GlkTextGridWindow class]])
        return ((GlkTextGridWindow *)win).speakableString;
    if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
        return ((GlkTextBufferWindow *)win).textview.string;
    }
    return nil;
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

    // Drop NULs everywhere first, so they can't shield whitespace at the
    // ends from the trim below.
    unichar nc = '\0';
    NSString *nullChar = [NSString stringWithCharacters:&nc length:1];
    newString = [newString stringByReplacingOccurrencesOfString:nullChar withString:@""];

    NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:@"\u00A0 >\n_\uFFFC"];
    newString = [newString stringByTrimmingCharactersInSet:charset];

    // VoiceOver silently drops an entire announcement that contains a
    // U+FFFC OBJECT REPLACEMENT CHARACTER. These come from inline images
    // (margin images, decorative drop-caps, illustrations) embedded in the
    // accessibility text -- stringFromRangeVal inserts a "(description)"
    // label next to each attachment but leaves the U+FFFC itself in place.
    // Trimming above only removes them from the ends, so any image in the
    // middle of a move (common on intro/rebus/graphics screens) silences the
    // whole announcement. Replace every occurrence with a space (so an image
    // sitting between two words can't fuse them) and the text is spoken.
    newString = [newString stringByReplacingOccurrencesOfString:@"\uFFFC" withString:@" "];

    if (newString.length == 0)
        newString = string;

    if (shouldAddTitlePrefixToSpeech) {
        newString = [NSString stringWithFormat:@"Now in, %@: %@", [self gameTitle], newString];
        shouldAddTitlePrefixToSpeech = NO;
    }

    [self announce:newString priority:NSAccessibilityPriorityHigh];
}

- (NSWindow *)announcementWindow {
    if (self.gameID == kGameIsJourney && Preferences.instance.currentGame == self.game)
        return NSApp.mainWindow;
    return self.window;
}

- (void)announce:(NSString *)string priority:(NSAccessibilityPriorityLevel)priority {
    if (string.length == 0)
        return;
    SPAnnounce(string, priority, self.announcementWindow);
}

- (GlkWindow *)largestWithMoves {
    // Find a "main text window"

// This somewhat reduces VoiceOver confusingly speaking of the command area grid in Journey
    if (self.gameID == kGameIsJourney) {
        GlkTextBufferWindow *bufwin = self.bufferWindows.firstObject;
        if (bufwin)
            return bufwin;
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
        for (GlkWindow *view in self.windowsIncludingQuoteBox) {
            if ([self speakableStringForWindow:view].length > 0)
                [windowsWithMoves addObject:view];
        }
        if (!windowsWithMoves.count) {
            // No windows with text
            return nil;
        }
    }

    return largestByFrameArea(windowsWithMoves);
}

// Every Glk window plus the quote box currently shown, if any. Quote boxes
// are not Glk windows, so they are not in gwindows.
- (NSArray<GlkWindow *> *)windowsIncludingQuoteBox {
    NSArray<GlkWindow *> *windows = self.gwindows.allValues;
    if (self.quoteBoxes.count)
        windows = [windows arrayByAddingObject:self.quoteBoxes.lastObject];
    return windows;
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

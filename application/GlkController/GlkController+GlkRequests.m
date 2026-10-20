#import "GlkController_Private.h"
#import "GlkController+Autorestore.h"
#import "GlkController+BorderColor.h"
#import "GlkController+InterpreterGlue.h"

#import "AppDelegate.h"
#import "BureaucracyForm.h"
#import "CommandScriptHandler.h"
#import "Constants.h"
#import "CoverImageHandler.h"
#import "FolderAccess.h"
#import "GlkGraphicsWindow.h"
#import "GlkEvent.h"
#import "GlkSoundChannel.h"
#import "GlkStyle.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "ImageHandler.h"
#import "Metadata.h"
#import "Preferences.h"
#import "SoundHandler.h"
#import "ZMenu.h"
#import "JourneyMenuHandler.h"
#import "InfocomV6MenuHandler.h"

#import "Game.h"
#import "Theme.h"

#import "NSColor+integer.h"
#import "NSString+Categories.h"

#include "glkimp.h"
#include "messagenames.h"
#include "protocol.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"

// Answer a file prompt: OKAY carrying the chosen path, or an empty payload
// if the user cancelled (path is NULL or "").
static void sendFilenameReply(int fd, const char *path) {
    struct message reply = { .cmd = OKAY };
    reply.len = path ? strlen(path) : 0;
    write(fd, &reply, sizeof(struct message));
    if (reply.len)
        write(fd, path, reply.len);
}

// Theme beep sounds. Stop first so a rapid repeat restarts the sound.
static void playSoundNamed(NSString *name) {
    if (!name)
        return;
    NSSound *sound = [NSSound soundNamed:name];
    [sound stop];
    [sound play];
}

@implementation GlkController (GlkRequests)

- (void)handleOpenPrompt:(int)fileusage {
    if (self.pendingSaveFilePath) {
        [self feedSaveFileToPrompt];
        self.pendingSaveFilePath = nil;
        return;
    }

    self.commandScriptHandler = nil;
    NSURL *directory =
    [NSURL fileURLWithPath:[[NSUserDefaults standardUserDefaults]
                            objectForKey:@"SaveDirectory"]
               isDirectory:YES];

    NSInteger sendfd = sendfh.fileDescriptor;

    // Create and configure the panel.
    NSOpenPanel *panel = [NSOpenPanel openPanel];

    self.mustBeQuiet = YES;

    if (fileusage == fileusage_SavedGame)
        panel.prompt = NSLocalizedString(@"Restore", nil);
    panel.directoryURL = directory;

    // Display the panel attached to the document's window.
    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        const char *s;

        if (result == NSModalResponseOK) {
            NSURL *theDoc = (panel.URLs)[0];

            [[NSUserDefaults standardUserDefaults]
             setObject:theDoc.path
                .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theDoc.path).fileSystemRepresentation;
        } else
            s = "";

        sendFilenameReply((int)sendfd, s);
        self.mustBeQuiet = NO;
        // Needed to prevent Journey dialog from popping up after closing file dialog
        if (self.gameID == kGameIsJourney)
            self.journeyMenuHandler.skipNextDialog = YES;
    }];

    [readfh waitForDataInBackgroundAndNotify];
}

- (void)feedSaveFileToPrompt {
    NSInteger sendfd = sendfh.fileDescriptor;
    [[NSUserDefaults standardUserDefaults]
     setObject:self.pendingSaveFilePath
        .stringByDeletingLastPathComponent
     forKey:@"SaveDirectory"];
    sendFilenameReply((int)sendfd, self.pendingSaveFilePath.fileSystemRepresentation);
    [readfh waitForDataInBackgroundAndNotify];
}

- (void)handleSavePrompt:(int)fileusage {
    self.commandScriptRunning = NO;
    self.commandScriptHandler = nil;
    NSURL *directory;
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:kSaveInGameDirectoryKey]) {
        if (!self.saveDir) {
            directory = [[self.game urlForBookmark] URLByDeletingLastPathComponent];
        } else {
            directory = self.saveDir;
        }
    } else {
        directory = [NSURL fileURLWithPath:
                     [defaults objectForKey:@"SaveDirectory"]
                               isDirectory:YES];
    }

    NSSavePanel *panel = [NSSavePanel savePanel];
    NSString *prompt;
    NSString *ext;
    NSString *filename;
    NSString *date;

    switch (fileusage) {
        case fileusage_Data:
            prompt = @"Save data file: ";
            ext = @"glkdata";
            filename = @"Data";
            break;
        case fileusage_SavedGame:
            prompt = @"Save game: ";
            ext = @"glksave";
            break;
        case fileusage_Transcript:
            prompt = @"Save transcript: ";
            ext = @"txt";
            filename = @"Transcript of ";
            break;
        case fileusage_InputRecord:
            prompt = @"Save recording: ";
            ext = @"rec";
            filename = @"Recording of ";
            break;
        default:
            prompt = @"Save: ";
            ext = nil;
            break;
    }

    panel.nameFieldLabel = NSLocalizedString(prompt, nil);
    if (ext)
        panel.allowedFileTypes = @[ ext ];
    panel.directoryURL = directory;

    panel.extensionHidden = NO;
    panel.canCreateDirectories = YES;

    if (fileusage == fileusage_Transcript || fileusage == fileusage_InputRecord)
        filename =
        [filename stringByAppendingString:self.game.metadata.title];

    if (fileusage == fileusage_SavedGame) {
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        formatter.dateFormat = @" yyyy-MM-dd HH.mm";
        date = [formatter stringFromDate:[NSDate date]];

        filename =
        [self.game.metadata.title stringByAppendingString:date];
    }

    if (ext)
        filename = [filename stringByAppendingPathExtension:ext];

    if (filename)
        panel.nameFieldStringValue = filename;

    NSInteger sendfd = sendfh.fileDescriptor;

    self.mustBeQuiet = YES;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        const char *s = NULL;

        if (result == NSModalResponseOK) {
            NSURL *theFile = panel.URL;
            if ([defaults boolForKey:kSaveInGameDirectoryKey]) {
                self.saveDir = theFile.URLByDeletingLastPathComponent;
            }
            [defaults
             setObject:theFile.path
                .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theFile.path).fileSystemRepresentation;
        }

        sendFilenameReply((int)sendfd, s);
        self.mustBeQuiet = NO;
    }];

    [readfh waitForDataInBackgroundAndNotify];
}

- (NSInteger)handleNewWindowOfType:(NSInteger)wintype andName:(NSInteger)name {
    NSInteger i;

    if (self.theme == nil) {
        NSLog(@"Theme nil?");
        self.theme = [Preferences currentTheme];
    }

    for (i = 0; i < kMaxGlkWindows; i++)
        if (self.gwindows[@(i)] == nil)
            break;

    if (i == kMaxGlkWindows)
        return -1;

    // If we are autorestoring, the window the interpreter process is asking us
    // to create may already exist. The we just return without doing anything.
    if (i != name) {
        NSLog(@"GlkController handleNewWindowOfType: Was asked to create a new "
              @"window with id %ld, but that already exists. First unused id "
              @"is %lu.",
              (long)name, (unsigned long)i);
        return name;
    }

    Class windowClass;

    switch (wintype) {
        case wintype_TextGrid:
            windowClass = [GlkTextGridWindow class];
            break;
        case wintype_TextBuffer:
            windowClass = [GlkTextBufferWindow class];
            break;
        case wintype_Graphics:
            windowClass = [GlkGraphicsWindow class];
            break;
        default:
            return -1;
    }

    GlkWindow *win = [(GlkWindow *)[windowClass alloc] initWithGlkController:self name:i];
    self.gwindows[@(i)] = win;
    [self.windowsToBeAdded addObject:win];
    return i;
}

- (void)handleSetTimer:(NSUInteger)millisecs {
    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    NSUInteger minTimer = (NSUInteger)self.theme.minTimer;

    if (millisecs > 0) {
        if (millisecs < minTimer) {
            //            NSLog(@"glkctl: too small timer interval (%ld); increasing to %lu",
            //                  (unsigned long)millisecs, (unsigned long)minTimer);
            millisecs = minTimer;
        }
        if (self.gameID == kGameIsKerkerkruip && millisecs == 10) {
            NSLog(@"Kerkerkruip tried to start a 10 millisecond timer.");
            return;
        }

        timer =
        [NSTimer scheduledTimerWithTimeInterval:(CGFloat)millisecs / 1000.0
                                         target:self
                                       selector:@selector(noteTimerTick:)
                                       userInfo:0
                                        repeats:YES];
    }
}

- (void)noteTimerTick:(id)sender {
    if (waitforevent) {
        GlkEvent *gevent = [[GlkEvent alloc] initTimerEvent];
        [self queueEvent:gevent];
    }
}

- (void)restartTimer:(id)sender {
    [self handleSetTimer:(NSUInteger)(self.storedTimerInterval * 1000)];
}

// Set (or, with NSNull, clear) a style hint in the grid and/or buffer table.
- (void)setStyleHintValue:(id)value
             onWindowType:(int)wintype
                    style:(NSUInteger)style
                     hint:(NSUInteger)hint {
    if (style >= style_NUMSTYLES || hint >= stylehint_NUMHINTS)
        return;

    if (wintype != wintype_AllTypes && wintype != wintype_TextGrid && wintype != wintype_TextBuffer) {
        NSLog(@"setStyleHintValue: unhandled wintype %d!", wintype);
        return;
    }

    if (wintype != wintype_TextBuffer) {
        NSMutableArray *gridHintsForStyle = self.gridStyleHints[style];
        gridHintsForStyle[hint] = value;
    }
    if (wintype != wintype_TextGrid) {
        NSMutableArray *bufferHintsForStyle = self.bufferStyleHints[style];
        bufferHintsForStyle[hint] = value;
    }
}

- (BOOL)handleStyleMeasureOnWin:(GlkWindow *)gwindow
                          style:(NSUInteger)style
                           hint:(NSUInteger)hint
                         result:(NSInteger *)result {

    // Measure the style table only (theme +/- stylehints applied at window
    // creation), not live zcolor or reverse video — same model as Gargoyle.
    NSDictionary *attributes = [gwindow baseAttributesForStyle:style];

    if (hint == stylehint_TextColor || hint == stylehint_BackColor) {
        NSColor *color = nil;
        if (hint == stylehint_TextColor) {
            color = attributes[NSForegroundColorAttributeName];
        }
        if (hint == stylehint_BackColor) {
            color = attributes[NSBackgroundColorAttributeName];
            if (!color) {
                color = [gwindow isKindOfClass:[GlkTextBufferWindow class]] ? self.theme.bufferBackground : self.theme.gridBackground;
            }
        }

        if (color) {
            *result = color.integerColor;
            return YES;
        }
    }

    if (hint == stylehint_Weight || hint == stylehint_Oblique ||
        hint == stylehint_Proportional || hint == stylehint_Size) {
        NSFont *font = attributes[NSFontAttributeName];
        if (!font)
            return NO;

        NSFontManager *fontManager = [NSFontManager sharedFontManager];
        NSFontTraitMask traits = [fontManager traitsOfFont:font];
        switch (hint) {
            case stylehint_Weight:
                // Glk: 1 bold, 0 normal, -1 light. NSFontManager weights run
                // 0-15 with 5 regular and 9 bold. Anything heavier than
                // regular counts as bold: a Weight 1 hint on Helvetica Neue
                // or Avenir Next lands on Medium (6), which carries no bold
                // trait but is the face the hint produced. Anything lighter
                // (Light, Thin, UltraLight) is the spec's -1.
                {
                    NSInteger weight = [fontManager weightOfFont:font];
                    if ((traits & NSBoldFontMask) || weight > 5)
                        *result = 1;
                    else if (weight < 5)
                        *result = -1;
                    else
                        *result = 0;
                }
                return YES;
            case stylehint_Oblique:
                *result = (traits & NSItalicFontMask) ? 1 : 0;
                return YES;
            case stylehint_Proportional:
                *result = ((traits & NSFixedPitchFontMask) || font.isFixedPitch) ? 0 : 1;
                return YES;
            case stylehint_Size:
                // Absolute size in density-independent pixels (AppKit points ≈ CSS px),
                // matching RemGlk/AsyncGlk's Math.round(computed.fontSize).
                *result = (NSInteger)llround(font.pointSize);
                return YES;
            default:
                break;
        }
    }

    if ([gwindow getStyleVal:style hint:hint value:result])
        return YES;

    return NO;
}


- (void)handlePrintOnWindow:(GlkWindow *)gwindow
                      style:(NSUInteger)style
                     buffer:(char *)rawbuf
                     length:(size_t)len {
    NSString *str;
    size_t bytesize = sizeof(unichar) * len;
    unichar *buf = malloc(bytesize);
    if (buf == NULL) {
        NSLog(@"Out of memory!");
        return;
    }

    memcpy(buf, rawbuf, bytesize);
    NSNumber *styleHintProportional = self.bufferStyleHints[style][stylehint_Proportional];
    BOOL proportional = ([gwindow isKindOfClass:[GlkTextBufferWindow class]] &&
                         (style & 0xff) != style_Preformatted &&
                         style != style_BlockQuote &&
                         ([styleHintProportional isEqual:[NSNull null]] ||
                          styleHintProportional.integerValue == 1));

    if (proportional) {
        GlkTextBufferWindow *textwin = (GlkTextBufferWindow *)gwindow;
        BOOL smartquotes = self.theme.smartQuotes;
        NSUInteger spaceformat = (NSUInteger)self.theme.spaceFormat;
        NSInteger lastchar = textwin.lastchar;
        NSInteger spaced = 0;
        size_t out = 0;

        // Rewrite buf in place; out trails in when characters are dropped
        for (size_t in = 0; in < len; in++) {
            unichar c = buf[in];

            /* turn (punct sp sp) into (punct sp) */
            if (spaceformat) {
                if (c == '.' || c == '!' || c == '?')
                    spaced = 1;
                else if (c == ' ' && spaced == 1)
                    spaced = 2;
                else if (c == ' ' && spaced == 2) {
                    spaced = 0;
                    continue;
                } else {
                    spaced = 0;
                }
            }

            if (smartquotes) {
                if (c == '`') {
                    c = 0x2018;
                } else if (c == '\'') {
                    c = (lastchar == ' ' || lastchar == '\n') ? 0x2018 : 0x2019;
                } else if (c == '"') {
                    c = (lastchar == ' ' || lastchar == '\n') ? 0x201c : 0x201d;
                } else if (c == '-' && out > 0 && (buf[out - 1] == '-' || buf[out - 1] == 0x2013)) {
                    /* "--" becomes an en dash, "---" an em dash */
                    buf[out - 1] = (buf[out - 1] == '-') ? 0x2013 : 0x2014;
                    lastchar = buf[out - 1];
                    continue;
                }
            }

            buf[out++] = c;
            lastchar = c;
        }
        len = out;
    }

    str = [NSString stringWithCharacters:buf length:len];
    [gwindow putString:str style:style];
    free(buf);
}

- (void)handleSetTerminatorsOnWindow:(GlkWindow *)gwindow
                              buffer:(glui32 *)buf
                              length:(glui32)len {
    NSMutableDictionary *myDict = gwindow.pendingTerminators;
    NSNumber *key;
    NSArray *keys = myDict.allKeys;

    for (key in keys) {
        myDict[key] = @(NO);
    }

    for (NSInteger i = 0; i < len; i++) {
        key = @(buf[i]);

        // Convert input terminator keys for Beyond Zork arrow keys hack
        if ((self.gameID == kGameIsBeyondZork || [self zVersion6]) && self.theme.bZTerminator == kBZArrowsSwapped && [gwindow isKindOfClass:[GlkTextBufferWindow class]]) {
            if (buf[i] == keycode_Up) {
                key = @(keycode_Home);
            } else if (buf[i] == keycode_Down) {
                key = @(keycode_End);
            }
        }

        id terminator_setting = myDict[key];
        if (terminator_setting) {
            myDict[key] = @(YES);
        } else {
            // Convert input terminator keys for Beyond Zork arrow keys hack
            if (self.gameID == kGameIsBeyondZork || [self zVersion6]) {
                if (self.theme.bZTerminator != kBZArrowsOriginal) {
                    if (buf[i] == keycode_Left) {
                        myDict[@"storedLeft"] = @(YES);
                    } else if (buf[i] == keycode_Right) {
                        myDict[@"storedRight"] = @(YES);
                    } else {
                        NSLog(@"Illegal line terminator request: %x", buf[i]);
                    }
                } else {
                    NSLog(@"Illegal line terminator request: %x", buf[i]);
                }
            } else {
                NSLog(@"Illegal line terminator request: %x", buf[i]);
            }
        }
    }
    gwindow.terminatorsPending = YES;
}

// SETTITLE and SHOWERROR send a UTF-8 string without a terminating NUL.
// The request buffer has room for one (see protocol.h).
static NSString *stringFromPayload(char *buf, size_t len) {
    buf[len] = '\0';
    return @(buf);
}

- (void)handleChangeTitle:(char *)buf length:(size_t)len {
    NSString *str = stringFromPayload(buf, len);
    if (str.length < 2)
        return;
    // ADRIFT games with no author-set name report the placeholder "Untitled"
    // (scarier's scr_get_game_name). Don't overwrite a sensible title with it.
    if ([str caseInsensitiveCompare:@"Untitled"] == NSOrderedSame)
        return;
    if ([self.game.metadata.title.lowercaseString isEqualToString:
         self.gamefile.lastPathComponent.lowercaseString]) {
        self.window.title = str;
        self.game.metadata.title = str;
    }
}

- (void)handleShowError:(char *)buf length:(size_t)len {
    NSString *str = stringFromPayload(buf, len);
    NSLog(@"handleShowError: %@ length: %zu", str, len);
    if (str.length < 2)
        return;
    if (self.theme.errorHandling == IGNORE_ERRORS) {
        self.pendingErrorMessage = str;
        self.errorTimeStamp = [NSDate date];
        return;
    }
    self.pendingErrorMessage = nil;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSAlert alertWithMessage:NSLocalizedString(@"ERROR", nil) info:str buttons:nil] runModal];
    });
}

- (NSUInteger)handleUnprintOnWindow:(GlkWindow *)win string:(unichar *)buf length:(size_t)len {
    NSString *str = [NSString stringWithCharacters:buf length:(NSUInteger)len];
    if (str == nil || len == 0)
        return 0;
    return [win unputString:str];
}

- (NSInteger)handleCanPrintGlyph:(glui32)glyph {
    // Checking means asking every installed font, and games tend to ask
    // about the same few characters over and over, so remember the answers.
    static NSMutableDictionary<NSNumber *, NSNumber *> *cache;
    if (!cache)
        cache = [[NSMutableDictionary alloc] init];
    NSNumber *cached = cache[@(glyph)];
    if (cached)
        return cached.boolValue;

    NSString *str = [[NSString alloc] initWithBytes:&glyph
                                             length:sizeof(glyph)
                                           encoding:NSUTF32LittleEndianStringEncoding];
    BOOL available = [GlkController unicodeAvailableForChar:str];
    cache[@(glyph)] = @(available);
    return available;
}

+ (BOOL)isCharacter:(NSString *)character supportedByFont:(NSString *)fontName
{
    if (character.length == 0 || character.length > 2) {
        return NO;
    }
    CTFontRef ctFont = CTFontCreateWithName((CFStringRef)fontName, 8, NULL);
    CGGlyph glyphs[2];
    BOOL ret = NO;
    UniChar characters[2];
    characters[0] = [character characterAtIndex:0];
    if(character.length == 2) {
        characters[1] = [character characterAtIndex:1];
    }
    ret = CTFontGetGlyphsForCharacters(ctFont, characters, glyphs, (CFIndex)character.length);
    CFRelease(ctFont);
    return ret;
}

+ (BOOL)unicodeAvailableForChar:(NSString *)charString {
    NSArray<NSString *> *fontlist = NSFontManager.sharedFontManager.availableFonts;
    for (NSString *fontName in fontlist) {
        if ([GlkController isCharacter:charString supportedByFont:fontName]) {
            return YES;
        }
    }
    return NO;
}

// The first window request after an autorestore rebuilds the UI from the
// late GUI autosave. Returns YES if it did, so the caller can look up its
// window again.
- (BOOL)restoreUIIfNeeded {
    if (self.gwindows.count || !shouldRestoreUI)
        return NO;
    self.windowsToRestore = restoredControllerLate.gwindows.allValues;
    [self restoreUI];
    return YES;
}

// A command script was running in the autosaved session. Step past the command that was being handled, or drop the script if that
// was its last one.
- (void)resumeRestoredCommandScript {
    CommandScriptHandler *handler = restoredController.commandScriptHandler;
    handler.commandIndex++;
    if (handler.commandIndex >= handler.commandArray.count) {
        restoredController.commandScriptHandler = nil;
        restoredControllerLate.commandScriptHandler = nil;
    } else {
        restoredControllerLate.commandScriptHandler = handler;
        skipNextScriptCommand = YES;
        lastScriptKeyTimestamp = [NSDate date];
    }
    restoredController = restoredControllerLate;
}

#pragma mark Per-request handlers

// The Glk window a request's a1 names, or nil.
- (GlkWindow *)windowForRequest:(struct message *)req {
    if (req->a1 >= 0 && req->a1 < kMaxGlkWindows)
        return self.gwindows[@(req->a1)];
    return nil;
}

// Fade out the oldest quote box once the game has printed or cleared past
// the point where it was added, or as soon as a newer one appears.
- (void)dismissExpiredQuoteBox {
    if (!self.quoteBoxes.count)
        return;
    if (self.quoteBoxes.lastObject.quoteboxAddedOnPAC == 0)
        self.quoteBoxes.lastObject.quoteboxAddedOnPAC = self.numberOfPrintsAndClears;
    if (self.numberOfPrintsAndClears <= self.quoteBoxes.lastObject.quoteboxAddedOnPAC &&
        self.quoteBoxes.count <= 1)
        return;

    GlkTextGridWindow *view = self.quoteBoxes.firstObject;
    [self.quoteBoxes removeObjectAtIndex:0];
    ((GlkTextBufferWindow *)view.quoteboxParent.superview).quoteBox = nil;
    view.quoteboxParent = nil;
    if (self.quoteBoxes.count == 0) {
        self.quoteBoxes = nil;
    }
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 1;
        view.animator.alphaValue = 0;
    } completionHandler:^{
        view.hidden = YES;
        view.alphaValue = 1;
        [view removeFromSuperview];
        view.glkctl = nil;
    }];
}

// Returns YES if the interpreter is now waiting for an event, so the caller
// should stop reading. A queued event is written here directly; otherwise
// an OKAY reply is set up when the interpreter asked not to wait (a1 == 0).
- (BOOL)handleNextEvent:(struct message *)req reply:(struct message *)ans {
    if (self.windowsToRestore.count) {
        for (GlkWindow *win in self.windowsToRestore) {
            if (![win isKindOfClass:[GlkGraphicsWindow class]])
                [self.gwindows[@(win.name)] postRestoreAdjustments:win];
        }
        self.windowsToRestore = nil;
    }
    // If this is the first turn, we try to restore the UI
    // from an autosave file.
    if (self.eventcount == 2) {
        if (shouldRestoreUI) {
            [self restoreUI];
        } else {
            // If we are not autorestoring, try to guess an input window.
            for (GlkTextBufferWindow *win in self.bufferWindows) {
                if (win.wantsFocus) {
                    [win grabFocus];
                }
            }
        }
    }

    if (self.eventcount > 1 && !self.shouldShowAutorestoreAlert) {
        self.mustBeQuiet = NO;
    }

    self.eventcount++;

    if (self.shouldSpeakNewText) {
        self.turns++;
        self.turnsThisSession++;
    }

    self.numberOfPrintsAndClears += self.printsAndClearsThisTurn;
    self.printsAndClearsThisTurn = 0;
    [self dismissExpiredQuoteBox];

    if (self.slowReadAlert != nil) {
        [self.slowReadAlert.window close];
        self.slowReadAlert = nil;
    }

    if (!autorestoring)
        [self flushDisplay];

    if (self.queue.count) {
        GlkEvent *gevent = self.queue[0];
        [gevent writeEvent:sendfh.fileDescriptor];
        [self.queue removeObjectAtIndex:0];
        return NO; /* keep reading ... we sent the reply */
    }

    if (!req->a1) {
        // Argument 1 is FALSE. No waiting for more events. Send a dummy
        // reply to hand over to the interpreter immediately.
        ans->cmd = OKAY;
        return NO;
    }

    [self guessFocus];

    waitforevent = YES;
    return YES; /* stop reading ... terp is waiting for reply */
}

- (void)handleDeleteWindow:(GlkWindow *)reqWin name:(int)name {
    if (reqWin) {
        [self.windowsToBeRemoved addObject:reqWin];
        [self.gwindows removeObjectForKey:@(name)];
        self.shouldCheckForMenu = YES;
    } else if (shouldRestoreUI && restoredController.gwindows[@(name)]) {
        // An autorestored window, closed by the interpreter before
        // restoreUI has brought its view back.
        if (!windowsClosedBeforeRestore)
            windowsClosedBeforeRestore = [[NSMutableSet alloc] init];
        [windowsClosedBeforeRestore addObject:@(name)];
    } else
        NSLog(@"delwin called on a non-existant Glk window (%d)", name);
}

- (void)handleBeep:(int)type frequency:(int)frequency duration:(int)duration {
    Theme *theme = self.theme;
    if (!theme.doSound)
        return;
    if (type == 0) {
        // Accurate ZX Spectrum tone: frequency in Hz, duration in ms.
        // Synthesised as a square wave.
        [self.soundHandler playSpectrumBeepFrequency:frequency
                                            duration:duration];
    }
    if (type == 1)
        playSoundNamed(theme.beepHigh);
    if (type == 2) {
        playSoundNamed(theme.beepLow);
        // For Bureaucracy form accessibility
        if (self.form)
            [self.form speakError];
    }
}

- (void)handleSizeWindow:(GlkWindow *)reqWin rect:(const struct sizewinrect *)sizewin {
    // Drop a request computed for a game view size we no longer have.
    if (fabs(sizewin->gamewidth - self.gameView.frame.size.width) > 1.0)
        return;

    if (fabs(sizewin->gameheight - self.gameView.frame.size.height) > 1.0)
        return;

    NSRect rect = NSMakeRect(sizewin->x0, sizewin->y0,
                             sizewin->x1 - sizewin->x0, sizewin->y1 - sizewin->y0);
    if (rect.size.width < 0)
        rect.size.width = 0;
    if (rect.size.height < 0)
        rect.size.height = 0;
    // Record layout churn so VoiceOver announcements wait until the
    // UI settles (see scheduleSpeakNewTextAfterDelay / fireSpeakNewText).
    if (!NSEqualRects(reqWin.frame, rect))
        self.lastLayoutChurnTimestamp = NSProcessInfo.processInfo.systemUptime;
    reqWin.frame = rect;

    NSAutoresizingMaskOptions hmask = NSViewMaxXMargin;
    NSAutoresizingMaskOptions vmask = NSViewMaxYMargin;

    if (fabs(NSMaxX(rect) - self.gameView.frame.size.width) < 2.0 &&
        rect.size.width > 0) {
        // If window is at right edge, attach to that edge
        hmask = NSViewWidthSizable;
    }

    // We special-case graphics windows, because the fullscreen animation looks
    // bad if they do not stay pinned to the top of the window.
    if (!([reqWin isKindOfClass:[GlkGraphicsWindow class]] && rect.origin.y == 0) &&
        (fabs(NSMaxY(rect) - self.gameView.frame.size.height) < 2.0 &&
         rect.size.height > 0)) {
        // Otherwise, if the Glk window is at the bottom,
        // pin it to the bottom of the main window
        vmask = NSViewHeightSizable;
    }

    reqWin.autoresizingMask = hmask | vmask;

    windowdirty = YES;
}

// win == -1 is the border (the area outside all Glk windows); a negative
// color means the theme's buffer background.
- (void)handleSetBackground:(GlkWindow *)reqWin window:(int)win color:(int)color {
    NSColor *bg = (color < 0) ? self.theme.bufferBackground : [NSColor colorFromInteger:color];
    if (win == -1) {
        self.lastAutoBGColor = bg;
        [self setBorderColor:bg];
        changedBorderThisTurn = YES;
    }

    if (reqWin) {
        [reqWin setBgColor:color];
    }
}

- (void)handleDrawImageOnWindow:(GlkWindow *)reqWin rect:(const struct drawrect *)drawstruct {
    NSImage *lastimage = self.imageHandler.lastimage;
    if (!lastimage || lastimage.size.width <= 0 || lastimage.size.height <= 0)
        return;
    [reqWin drawImage:lastimage
                 val1:(glsi32)drawstruct->x
                 val2:(glsi32)drawstruct->y
                width:drawstruct->width
               height:drawstruct->height
            imagerule:drawstruct->imagerule
             maxwidth:drawstruct->maxwidth
                style:drawstruct->style];
}

- (void)handleInitLine:(struct message *)req buffer:(char *)buf {
    [self performScroll];

    // Text to preload the input line with. Dropped when restoring
    // the UI: the restored window already holds its input.
    NSUInteger preloadLength = (NSUInteger)req->len / sizeof(unichar);
    if ([self restoreUIIfNeeded])
        preloadLength = 0;
    GlkWindow *reqWin = [self windowForRequest:req];

    if (reqWin && self.gameID != kGameIsAColderLight && !skipNextScriptCommand) {
        NSString *preloaded = [NSString stringWithCharacters:(unichar *)buf length:preloadLength];
        if (!preloaded.length || [preloaded characterAtIndex:0] == '\0')
            preloaded = @"";
        [reqWin initLine:preloaded maxLength:(NSUInteger)req->a2];

        if (self.commandScriptRunning) {
            [self.commandScriptHandler sendCommandLineToWindow:reqWin];
        }

        self.shouldSpeakNewText = YES;

        // Check if we are in Beyond Zork Definitions menu
        if (self.gameID == kGameIsBeyondZork)
            self.shouldCheckForMenu = YES;
    }
    skipNextScriptCommand = NO;
}

- (void)handleCancelLine:(GlkWindow *)reqWin reply:(struct message *)ans buffer:(char *)buf {
    ans->cmd = OKAY;
    if (!reqWin)
        return;
    NSString *str = reqWin.cancelLine;
    // buf holds GLKBUFSIZE bytes: copy no more than the reply
    // carries, or a very long line would run past it.
    NSUInteger units = MIN(str.length, GLKBUFSIZE / sizeof(unichar));
    ans->len = units * sizeof(unichar);
    [str getCharacters:(unsigned short *)buf
                 range:NSMakeRange(0, units)];
}

- (void)handleInitChar:(struct message *)req {
    if (!self.gwindows.count && shouldRestoreUI && restoredControllerLate.commandScriptRunning)
        [self resumeRestoredCommandScript];
    [self restoreUIIfNeeded];
    GlkWindow *reqWin = [self windowForRequest:req];

    // Distinguish a meaningful INITCHAR (one that follows real game
    // output) from one issued by a rapid CANCELCHAR/INITCHAR cycle
    // some games use as a polling timer (notably the Level 9 Adrian
    // Mole / Archers games). Only treat this INITCHAR as a real
    // input prompt — for scrolling, VoiceOver, and menu detection —
    // if a real output request preceded it, or it has been more
    // than a second since the last keystroke.
    if (lastRequest == PRINT ||
        lastRequest == SETZCOLOR ||
        lastRequest == NEXTEVENT ||
        lastRequest == MOVETO ||
        lastKeyTimestamp.timeIntervalSinceNow < -1) {
        // This flag may be set by GlkBufferWindow as well
        self.shouldScrollOnCharEvent = YES;
        self.shouldSpeakNewText = YES;
        self.shouldCheckForMenu = YES;
    }

    lastKeyTimestamp = [NSDate date];

    if (self.shouldScrollOnCharEvent) {
        [self performScroll];
    }

    if (reqWin && !skipNextScriptCommand) {
        [reqWin initChar];
        if (self.commandScriptRunning) {
            if (self.gameID != kGameIsAdrianMole || lastScriptKeyTimestamp.timeIntervalSinceNow < -0.5) {
                [self.commandScriptHandler sendCommandKeyPressToWindow:reqWin];
                lastScriptKeyTimestamp = [NSDate date];
            }
        }
    }
    skipNextScriptCommand = NO;
}

// This updates an existing window on-the-fly with the styles that normally
// would only be applied to a new window. It can also update any inline
// images (xscale/yscale in thousandths; 0 leaves them alone).
- (void)handleRefreshWindow:(GlkWindow *)reqWin xscale:(int)xscale yscale:(int)yscale {
    if ([reqWin isKindOfClass:[GlkTextBufferWindow class]]) {
        reqWin.styleHints = [GlkWindow deepCopyOfStyleHintsArray:self.bufferStyleHints];
        if (xscale > 0)
            [((GlkTextBufferWindow *)reqWin) updateImageAttachmentsWithXScale:xscale / 1000.0 yScale:yscale / 1000.0];
    } else if ([reqWin isKindOfClass:[GlkTextGridWindow class]]) {
        reqWin.styleHints = [GlkWindow deepCopyOfStyleHintsArray:self.gridStyleHints];
    } else {
        return;
    }
    [self flushDisplay];
    [reqWin prefsDidChange];
}

- (void)handleMenuItem:(struct message *)req buffer:(char *)buf {
    if (self.gameID == kGameIsJourney) {
        [self.journeyMenuHandler handleMenuItemOfType:(JourneyMenuType)req->a1 column:(NSUInteger)req->a2 line:(NSUInteger)req->a3 stopflag:(req->a4 == 1) text:buf length:(NSUInteger)req->len];
    } else {
        [self.infocomV6MenuHandler handleMenuItemOfType:(InfocomV6MenuType)req->a1 index:(NSUInteger)req->a2 total:(NSUInteger)req->a3 text:buf length:(NSUInteger)req->len];
    }
}

#pragma mark Request dispatch

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {
    BOOL stop = NO;
    GlkWindow *reqWin = [self windowForRequest:req];

    switch (req->cmd) {
        case HELLO:
            ans->cmd = OKAY;
            ans->a1 = self.theme.doGraphics;
            ans->a2 = self.theme.doSound;
            break;

        case NEXTEVENT:
            stop = [self handleNextEvent:req reply:ans];
            break;

        case PROMPTOPEN:
            // Flush first so any text the game printed immediately before the
            // prompt (e.g. a "Restore which game?" message) is on screen before
            // the file panel appears, not after it is dismissed.
            [self flushDisplay];
            [self handleOpenPrompt:req->a1];
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTSAVE:
            [self flushDisplay];
            [self handleSavePrompt:req->a1];
            return YES; /* stop reading ... terp is waiting for reply */

        case STYLEHINT:
            [self setStyleHintValue:@(req->a4)
                       onWindowType:req->a1
                              style:(NSUInteger)req->a2
                               hint:(NSUInteger)req->a3];
            break;

        case STYLEMEASURE: {
            NSInteger result = 0;
            ans->cmd = OKAY;
            ans->a1 = [self handleStyleMeasureOnWin:reqWin
                                              style:(NSUInteger)req->a2
                                               hint:(NSUInteger)req->a3
                                             result:&result];
            ans->a2 = (int)result;
            break;
        }

        case CLEARHINT:
            [self setStyleHintValue:[NSNull null]
                       onWindowType:req->a1
                              style:(NSUInteger)req->a2
                               hint:(NSUInteger)req->a3];
            break;

        // Create and destroy windows and sound channels

        case NEWWIN:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleNewWindowOfType:req->a1 andName:req->a2];
            break;

        case NEWCHAN:
            ans->cmd = OKAY;
            ans->a1 = [self.soundHandler handleNewSoundChannel:(glui32)req->a1];
            break;

        case DELWIN:
            [self handleDeleteWindow:reqWin name:req->a1];
            break;

        case DELCHAN:
            [self.soundHandler handleDeleteChannel:req->a1];
            break;

        // Load images; load and play sounds

        case FINDIMAGE:
            ans->cmd = OKAY;
            ans->a1 = [self.imageHandler handleFindImageNumber:req->a1];
            break;

        case LOADIMAGE:
            buf[req->len] = 0;
            [self.imageHandler handleLoadImageNumber:req->a1
                                                from:@(buf)
                                              offset:(NSUInteger)req->a2
                                                size:(NSUInteger)req->a3];
            break;

        case SIZEIMAGE: {
            ans->cmd = OKAY;
            NSImage *lastimage = self.imageHandler.lastimage;
            if (lastimage) {
                ans->a1 = (int)lastimage.size.width;
                ans->a2 = (int)lastimage.size.height;
            } else {
                NSLog(@"SIZEIMAGE: No last image found!");
            }
            break;
        }

        case FINDSOUND:
            ans->cmd = OKAY;
            ans->a1 = [self.soundHandler handleFindSoundNumber:req->a1];
            break;

        case LOADSOUND:
            buf[req->len] = 0;
            [self.soundHandler handleLoadSoundNumber:req->a1
                                                from:@(buf)
                                              offset:(NSUInteger)req->a2
                                              length:(NSUInteger)req->a3];
            break;

        case SETVOLUME:
            [self.soundHandler handleSetVolume:(glui32)req->a2
                                       channel:req->a1
                                      duration:(glui32)req->a3
                                        notify:(glui32)req->a4];
            break;

        case PLAYSOUND:
            [self.soundHandler handlePlaySoundOnChannel:req->a1 repeats:(glsi32)req->a2 notify:(glui32)req->a3];
            break;

        case STOPSOUND:
            [self.soundHandler handleStopSoundOnChannel:req->a1];
            break;

        case PAUSE:
            [self.soundHandler handlePauseOnChannel:req->a1];
            break;

        case UNPAUSE:
            [self.soundHandler handleUnpauseOnChannel:req->a1];
            break;

        case BEEP:
            [self handleBeep:req->a1 frequency:req->a2 duration:req->a3];
            break;

        // Window sizing, printing, drawing, etc.

        case SHOWERROR:
            [self handleShowError:buf length:req->len];
            break;

        case CANPRINT:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleCanPrintGlyph:(glui32)req->a1];
            break;

        case SIZWIN:
            if (reqWin) {
                // Copy out: buf need not be aligned for the struct.
                struct sizewinrect sizewin;
                memcpy(&sizewin, buf, sizeof sizewin);
                [self handleSizeWindow:reqWin rect:&sizewin];
            } else
                NSLog(@"sizwin called on a non-existant Glk window (%d)", req->a1);
            break;

        case CLRWIN:
            self.printsAndClearsThisTurn++;
            if (reqWin) {
                [reqWin clear];
                self.shouldCheckForMenu = YES;
            }
            break;

        case SETBGND:
            [self handleSetBackground:reqWin window:req->a1 color:req->a2];
            break;

        case DRAWIMAGE:
            if (reqWin) {
                struct drawrect drawstruct;
                memcpy(&drawstruct, buf, sizeof drawstruct);
                [self handleDrawImageOnWindow:reqWin rect:&drawstruct];
            }
            break;

        case FILLRECT:
            if (reqWin) {
                [reqWin fillRects:(struct fillrect *)buf count:req->a2];
            }
            break;

        case PRINT:
            self.printsAndClearsThisTurn++;
            if ([self restoreUIIfNeeded])
                reqWin = [self windowForRequest:req];
            if (reqWin && req->len) {
                [self handlePrintOnWindow:reqWin
                                    style:(NSUInteger)req->a2
                                   buffer:buf
                                   length:req->len / sizeof(unichar)];
            } else {
                NSLog(@"Print to non-existent window!");
            }
            break;

        case UNPRINT:
            if ([self restoreUIIfNeeded])
                reqWin = [self windowForRequest:req];
            ans->cmd = OKAY;
            if (reqWin && req->len) {
                ans->a1 = (int)[self handleUnprintOnWindow:reqWin string:(unichar *)buf length:req->len / sizeof(unichar)];
            }
            break;

        case MOVETO:
            if (reqWin) {
                [reqWin moveToColumn:(NSUInteger)req->a2 row:(NSUInteger)req->a3];
            }
            break;

        case SETECHO:
            if ([reqWin isKindOfClass:[GlkTextBufferWindow class]])
                [(GlkTextBufferWindow *)reqWin echo:(req->a2 != 0)];
            break;

        case TERMINATORS:
            [self handleSetTerminatorsOnWindow:reqWin
                                        buffer:(glui32 *)buf
                                        length:(glui32)req->a2];
            break;

        case FLOWBREAK:
            if (reqWin) {
                [reqWin flowBreak];
            }
            break;

        case SETZCOLOR:
            if (reqWin) {
                [reqWin setZColorText:(glui32)req->a2 background:(glui32)req->a3];
            }
            break;

        case SETREVERSE:
            if (reqWin) {
                reqWin.currentReverseVideo = (req->a2 != 0);
            }
            break;

        case QUOTEBOX:
            if (reqWin) {
                [((GlkTextGridWindow *)reqWin) quotebox:(NSUInteger)req->a2];
            }
            break;

        // Request and cancel events

        case INITLINE:
            [self handleInitLine:req buffer:buf];
            break;

        case CANCELLINE:
            [self handleCancelLine:reqWin reply:ans buffer:buf];
            break;

        case INITCHAR:
            [self handleInitChar:req];
            break;

        case CANCELCHAR:
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
            if ([self restoreUIIfNeeded])
                reqWin = [self windowForRequest:req];
            [self performScroll];
            if (reqWin) {
                [reqWin initMouse];
                self.shouldSpeakNewText = YES;
            }
            break;

        case CANCELMOUSE:
            if (reqWin)
                [reqWin cancelMouse];
            break;

        case SETLINK:
            if (reqWin) {
                reqWin.currentHyperlink = req->a2;
            }
            break;

        case INITLINK:
            if ([self restoreUIIfNeeded])
                reqWin = [self windowForRequest:req];
            [self performScroll];
            if (reqWin) {
                [reqWin initHyperlink];
                self.shouldSpeakNewText = YES;
            }
            break;

        case CANCELLINK:
            if (reqWin) {
                [reqWin cancelHyperlink];
            }
            break;

        case TIMER:
            [self handleSetTimer:(NSUInteger)req->a1];
            break;

        // Non-standard Glk extensions

        case MAKETRANSPARENT:
            if (reqWin)
                [reqWin makeTransparent];
            break;

        case SETTITLE:
            [self handleChangeTitle:buf length:req->len];
            break;

        case AUTOSAVE:
            [self handleAutosave:req->a2];
            break;

            // This just kills the interpreter process and restarts it from scratch.
            // Used if an autorestore fails.
        case RESET:
            [self reset:nil];
            break;

            // Used by Tads 3 to adapt the banner width and height.
        case BANNERCOLS:
        case BANNERLINES:
            ans->cmd = OKAY;
            if ([reqWin isKindOfClass:[GlkTextBufferWindow class]]) {
                GlkTextBufferWindow *banner = (GlkTextBufferWindow *)reqWin;
                ans->a1 = (int)(req->cmd == BANNERCOLS ? banner.numberOfColumns : banner.numberOfLines);
            }
            break;

        case PURGEIMG:
            if (req->len) {
                buf[req->len] = 0;
                [self.imageHandler purgeImage:req->a1 withReplacement:@(buf)];
            } else {
                [self.imageHandler purgeImage:req->a1 withReplacement:nil];
            }
            break;

        case REFRESH:
            [self handleRefreshWindow:reqWin xscale:req->a2 yscale:req->a3];
            break;

        case MENUITEM:
            [self handleMenuItem:req buffer:buf];
            break;

        default:
            NSLog(@"glkctl: unhandled request (%d)", req->cmd);
            break;
    }

    if (req->cmd != AUTOSAVE)
        lastRequest = req->cmd;
    return stop; /* if YES, stop reading: terp is waiting for reply */
}

@end

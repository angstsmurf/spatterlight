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
#include "protocol.h"

#ifndef DEBUG
#define NSLog(...)
#endif

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

    waitforfilename = YES; /* don't interrupt */

    if (fileusage == fileusage_SavedGame)
        panel.prompt = NSLocalizedString(@"Restore", nil);
    panel.directoryURL = directory;

    // Display the panel attached to the document's window.
    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        const char *s;
        struct message reply;

        if (result == NSModalResponseOK) {
            NSURL *theDoc = (panel.URLs)[0];

            [[NSUserDefaults standardUserDefaults]
             setObject:theDoc.path
                .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theDoc.path).fileSystemRepresentation;
        } else
            s = "";

        reply.cmd = OKAY;
        reply.len = strlen(s);

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
        self.mustBeQuiet = NO;
        // Needed to prevent Journey dialog from popping up after closing file dialog
        if (self.gameID == kGameIsJourney)
            self.journeyMenuHandler.skipNextDialog = YES;
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (void)feedSaveFileToPrompt {
    NSInteger sendfd = sendfh.fileDescriptor;
    [[NSUserDefaults standardUserDefaults]
     setObject:self.pendingSaveFilePath
        .stringByDeletingLastPathComponent
     forKey:@"SaveDirectory"];
    const char *s = self.pendingSaveFilePath.fileSystemRepresentation;
    struct message reply;
    reply.cmd = OKAY;
    reply.len = strlen(s);
    write((int)sendfd, &reply, sizeof(struct message));
    if (reply.len)
        write((int)sendfd,s, reply.len);
    [readfh waitForDataInBackgroundAndNotify];
}

- (void)handleSavePrompt:(int)fileusage {
    self.commandScriptRunning = NO;
    self.commandScriptHandler = nil;
    NSURL *directory;
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:@"SaveInGameDirectory"]) {
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

    waitforfilename = YES; /* don't interrupt */

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
        struct message reply;
        const char *s;

        if (result == NSModalResponseOK) {
            NSURL *theFile = panel.URL;
            if ([defaults boolForKey:@"SaveInGameDirectory"]) {
                self.saveDir = theFile.URLByDeletingLastPathComponent;
            }
            [defaults
             setObject:theFile.path
                .stringByDeletingLastPathComponent
             forKey:@"SaveDirectory"];
            s = (theFile.path).fileSystemRepresentation;
            reply.len = strlen(s);
        } else {
            s = nil;
            reply.len = 0;
        }

        reply.cmd = OKAY;

        write((int)sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write((int)sendfd, s, reply.len);
        self.mustBeQuiet = NO;
    }];

    waitforfilename = NO; /* we're all done, resume normal processing */

    [readfh waitForDataInBackgroundAndNotify];
}

- (NSInteger)handleNewWindowOfType:(NSInteger)wintype andName:(NSInteger)name {
    NSInteger i;

    if (self.theme == nil) {
        NSLog(@"Theme nil?");
        self.theme = [Preferences currentTheme];
    }

    for (i = 0; i < MAXWIN; i++)
        if (self.gwindows[@(i)] == nil)
            break;

    if (i == MAXWIN)
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

    GlkWindow *win;

    switch (wintype) {
        case wintype_TextGrid:
            win = [[GlkTextGridWindow alloc] initWithGlkController:self name:i];
            self.gwindows[@(i)] = win;
            [self.windowsToBeAdded addObject:win];
            return i;

        case wintype_TextBuffer:
            win = [[GlkTextBufferWindow alloc] initWithGlkController:self name:i];

            self.gwindows[@(i)] = win;
            [self.windowsToBeAdded addObject:win];
            return i;

        case wintype_Graphics:
            win = [[GlkGraphicsWindow alloc] initWithGlkController:self name:i];
            self.gwindows[@(i)] = win;
            [self.windowsToBeAdded addObject:win];
            return i;
    }

    return -1;
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
            [timer invalidate];
            NSLog(@"Kerkerkruip tried to start a 10 millisecond timer.");
            return;
        }

        timer =
        [NSTimer scheduledTimerWithTimeInterval:millisecs / 1000.0
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

- (void)handleStyleHintOnWindowType:(int)wintype
                              style:(NSUInteger)style
                               hint:(NSUInteger)hint
                              value:(int)value {
    if (style >= style_NUMSTYLES)
        return;

    if (hint >= stylehint_NUMHINTS)
        return;

    NSMutableArray *bufferHintsForStyle = self.bufferStyleHints[style];
    NSMutableArray *gridHintsForStyle = self.gridStyleHints[style];

    switch (wintype) {
        case wintype_AllTypes:
            gridHintsForStyle[hint] = @(value);
            bufferHintsForStyle[hint] = @(value);
            break;
        case wintype_TextGrid:
            gridHintsForStyle[hint] = @(value);
            break;
        case wintype_TextBuffer:
            bufferHintsForStyle[hint] = @(value);
            break;
        default:
            NSLog(@"styleHintOnWindowType for unhandled wintype!");
            break;
    }
}

- (BOOL)handleStyleMeasureOnWin:(GlkWindow *)gwindow
                          style:(NSUInteger)style
                           hint:(NSUInteger)hint
                         result:(NSInteger *)result {

    if (hint == stylehint_TextColor || hint == stylehint_BackColor) {
        NSMutableDictionary *attributes = [gwindow getCurrentAttributesForStyle:style];
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

    if ([gwindow getStyleVal:style hint:hint value:result])
        return YES;

    return NO;
}

- (void)handleClearHintOnWindowType:(int)wintype
                              style:(NSUInteger)style
                               hint:(NSUInteger)hint {

    if (style >= style_NUMSTYLES)
        return;

    NSMutableArray *gridHintsForStyle = self.gridStyleHints[style];
    NSMutableArray *bufferHintsForStyle = self.bufferStyleHints[style];

    switch (wintype) {
        case wintype_AllTypes:
            gridHintsForStyle[hint] = [NSNull null];
            bufferHintsForStyle[hint] = [NSNull null];
            break;
        case wintype_TextGrid:
            gridHintsForStyle[hint] = [NSNull null];
            break;
        case wintype_TextBuffer:
            bufferHintsForStyle[hint] = [NSNull null];
            break;
        default:
            NSLog(@"clearHintOnWindowType for unhandled wintype!");
            break;
    }

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
                         ([styleHintProportional isEqualTo:[NSNull null]] ||
                          styleHintProportional.integerValue == 1));

    if (proportional) {
        GlkTextBufferWindow *textwin = (GlkTextBufferWindow *)gwindow;
        BOOL smartquotes = self.theme.smartQuotes;
        NSUInteger spaceformat = (NSUInteger)self.theme.spaceFormat;
        NSInteger lastchar = textwin.lastchar;
        NSInteger spaced = 0;
        NSUInteger i;

        for (i = 0; i < len; i++) {
            /* turn (punct sp sp) into (punct sp) */
            if (spaceformat) {
                if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?')
                    spaced = 1;
                else if (buf[i] == ' ' && spaced == 1)
                    spaced = 2;
                else if (buf[i] == ' ' && spaced == 2) {
                    memmove(buf + i, buf + i + 1,
                            (len - (i + 1)) * sizeof(unichar));
                    len--;
                    i--;
                    spaced = 0;
                } else {
                    spaced = 0;
                }
            }

            if (smartquotes && buf[i] == '`')
                buf[i] = 0x2018;

            else if (smartquotes && buf[i] == '\'') {
                if (lastchar == ' ' || lastchar == '\n')
                    buf[i] = 0x2018;
                else
                    buf[i] = 0x2019;
            }

            else if (smartquotes && buf[i] == '"') {
                if (lastchar == ' ' || lastchar == '\n')
                    buf[i] = 0x201c;
                else
                    buf[i] = 0x201d;
            }

            else if (smartquotes && i > 1 && buf[i - 1] == '-' &&
                     buf[i] == '-') {
                memmove(buf + i, buf + i + 1,
                        (len - (i + 1)) * sizeof(unichar));
                len--;
                i--;
                buf[i] = 0x2013;
            }

            else if (smartquotes && i > 1 && buf[i - 1] == 0x2013 &&
                     buf[i] == '-') {
                memmove(buf + i, buf + i + 1,
                        (len - (i + 1)) * sizeof(unichar));
                len--;
                i--;
                buf[i] = 0x2014;
            }

            lastchar = buf[i];
        }

        len = (size_t)i;
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

- (void)handleChangeTitle:(char *)buf length:(size_t)len {
    buf[len] = '\0';
    NSLog(@"HandleChangeTitle: %s length: %zu", buf, len);
    NSString *str = @(buf);
    if (str && str.length > (NSUInteger)len - 1)
        [@(buf) substringToIndex:(NSUInteger)len - 1];
    if (str == nil || str.length < 2)
        return;
    if ([self.game.metadata.title.lowercaseString isEqualToString:
         self.gamefile.lastPathComponent.lowercaseString]) {
        self.window.title = str;
        self.game.metadata.title = str;
    }
}

- (void)handleShowError:(char *)buf length:(size_t)len {
    buf[len] = '\0';
    NSLog(@"handleShowError: %s length: %zu", buf, len);
    NSString *str = @(buf);
    if (str && str.length > (NSUInteger)len - 1)
        [@(buf) substringToIndex:(NSUInteger)len - 1];
    if (str == nil || str.length < 2)
        return;
    if (self.theme.errorHandling == IGNORE_ERRORS) {
        self.pendingErrorMessage = str;
        self.errorTimeStamp = [NSDate date];
        return;
    }
    self.pendingErrorMessage = nil;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"ERROR", nil);
        alert.informativeText = str;
        [alert runModal];
    });
}

- (NSUInteger)handleUnprintOnWindow:(GlkWindow *)win string:(unichar *)buf length:(size_t)len {
    NSString *str = [NSString stringWithCharacters:buf length:(NSUInteger)len];
    if (str == nil || len == 0)
        return 0;
    return [win unputString:str];
}

- (NSInteger)handleCanPrintGlyph:(glui32)glyph {
    glui32 uniglyph[1];
    uniglyph[0] = glyph;
    NSData *data = [NSData dataWithBytes:uniglyph length:4];
    NSString *str  = [[NSString alloc] initWithData:data
                                           encoding:NSUTF32LittleEndianStringEncoding];
    return [GlkController unicodeAvailableForChar:str];
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

- (nullable JourneyMenuHandler *)journeyMenuHandler {
    if (_journeyMenuHandler == nil) {
        GlkTextGridWindow *gridwindow = nil;
        GlkTextBufferWindow *bufferwindow = nil;
        for (GlkWindow *win in self.gwindows.allValues)
            if ([win isKindOfClass:[GlkTextGridWindow class]]) {
                gridwindow = (GlkTextGridWindow *)win;
            } else if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                bufferwindow = (GlkTextBufferWindow *)win;
            }
        if (gridwindow == nil || bufferwindow == nil)
            return nil;
        _journeyMenuHandler = [[JourneyMenuHandler alloc] initWithDelegate:self gridWindow:gridwindow bufferWindow:bufferwindow];
    }
    return _journeyMenuHandler;
}

- (nullable InfocomV6MenuHandler *)infocomV6MenuHandler {
    if (_infocomV6MenuHandler == nil) {

       _infocomV6MenuHandler = [[InfocomV6MenuHandler alloc] initWithDelegate:self];
    }
    return _infocomV6MenuHandler;
}

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {
//    NSLog(@"glkctl: incoming request %s", msgnames[req->cmd]);

    NSInteger result;
    GlkWindow *reqWin = nil;
    NSColor *bg = nil;

    Theme *theme = self.theme;

    if (req->a1 >= 0 && req->a1 < MAXWIN && self.gwindows[@(req->a1)])
        reqWin = self.gwindows[@(req->a1)];

    switch (req->cmd) {
        case HELLO:
            ans->cmd = OKAY;
            ans->a1 = theme.doGraphics;
            ans->a2 = theme.doSound;
            break;

        case NEXTEVENT:
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
                    for (GlkWindow *win in self.gwindows.allValues) {
                        if ([win isKindOfClass:[GlkTextBufferWindow class]] && win.wantsFocus) {
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
            }

            self.numberOfPrintsAndClears += self.printsAndClearsThisTurn;
            self.printsAndClearsThisTurn = 0;
            if (self.quoteBoxes.count) {
                if (self.quoteBoxes.lastObject.quoteboxAddedOnPAC == 0)
                    self.quoteBoxes.lastObject.quoteboxAddedOnPAC = self.numberOfPrintsAndClears;
                if (self.numberOfPrintsAndClears > self.quoteBoxes.lastObject.quoteboxAddedOnPAC ||
                    self.quoteBoxes.count > 1) {
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
            }

            if (self.slowReadAlert != nil) {
                [self.slowReadAlert.window close];
                self.slowReadAlert = nil;
            }

            if (!autorestoring)
                [self flushDisplay];

            if (self.queue.count) {
                GlkEvent *gevent;
                gevent = self.queue[0];

                [gevent writeEvent:sendfh.fileDescriptor];
                [self.queue removeObjectAtIndex:0];
                lastRequest = req->cmd;
                return NO; /* keep reading ... we sent the reply */
            } else {
                // No queued events.

                if (!req->a1) {
                    // Argument 1 is FALSE. No waiting for more events. Send a dummy
                    // reply to hand over to the interpreter immediately.
                    ans->cmd = OKAY;
                    break;
                }
            }

            [self guessFocus];

            waitforevent = YES;
            lastRequest = req->cmd;
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTOPEN:
            [self handleOpenPrompt:req->a1];
            [self flushDisplay];
            return YES; /* stop reading ... terp is waiting for reply */

        case PROMPTSAVE:
            [self handleSavePrompt:req->a1];
            [self flushDisplay];
            return YES; /* stop reading ... terp is waiting for reply */

        case STYLEHINT:
            [self handleStyleHintOnWindowType:req->a1
                                        style:(NSUInteger)req->a2
                                         hint:(NSUInteger)req->a3
                                        value:req->a4];
            break;

        case STYLEMEASURE:
            result = 0;
            ans->cmd = OKAY;
            ans->a1 = [self handleStyleMeasureOnWin:reqWin
                                              style:(NSUInteger)req->a2
                                               hint:(NSUInteger)req->a3
                                             result:&result];
            ans->a2 = (int)result;
            break;

        case CLEARHINT:
            [self handleClearHintOnWindowType:req->a1 style:(NSUInteger)req->a2 hint:(NSUInteger)req->a3];
            break;

            /*
             * Create and destroy windows and channels
             */

#pragma mark Create and destroy windows and sound channels

        case NEWWIN:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleNewWindowOfType:req->a1 andName:req->a2];
            break;

        case NEWCHAN:
            ans->cmd = OKAY;
            ans->a1 = [self.soundHandler handleNewSoundChannel:(glui32)req->a1];
            break;

        case DELWIN:
            if (reqWin) {
                [self.windowsToBeRemoved addObject:reqWin];
                [self.gwindows removeObjectForKey:@(req->a1)];
                self.shouldCheckForMenu = YES;
            } else
                NSLog(@"delwin called on a non-existant Glk window (%d)", req->a1);

            break;

        case DELCHAN:
            [self.soundHandler handleDeleteChannel:req->a1];
            break;

            /*
             * Load images; load and play sounds
             */

#pragma mark Load images; load and play sounds

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

        case SIZEIMAGE:
        {
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            NSImage *lastimage = self.imageHandler.lastimage;
            if (lastimage) {
                NSSize size;
                size = lastimage.size;
                ans->a1 = (int)size.width;
                ans->a2 = (int)size.height;
            } else {
                NSLog(@"SIZEIMAGE: No last image found!");
            }
        }
            break;

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
            if (theme.doSound) {
                if (req->a1 == 1 && theme.beepHigh) {
                    NSSound *sound = [NSSound soundNamed:theme.beepHigh];
                    if (sound) {
                        [sound stop];
                        [sound play];
                    }
                }
                if (req->a1 == 2 && theme.beepLow) {
                    NSSound *sound = [NSSound soundNamed:theme.beepLow];
                    if (sound) {
                        [sound stop];
                        [sound play];
                    }
                    // For Bureaucracy form accessibility
                    if (self.form)
                        [self.form speakError];
                }
            }

            break;
            /*
             * Window sizing, printing, drawing, etc...
             */

#pragma mark Window sizing, printing, drawing …
        case SHOWERROR:
            [self handleShowError:(char *)buf length:req->len];
            break;

        case CANPRINT:
            ans->cmd = OKAY;
            ans->a1 = (int)[self handleCanPrintGlyph:(glui32)req->a1];
            break;

        case SIZWIN:
            if (reqWin) {
                int x0, y0, x1, y1, checksumWidth, checksumHeight;
                NSRect rect;

                struct sizewinrect *sizewin = malloc(sizeof(struct sizewinrect));

                memcpy(sizewin, buf, sizeof(struct sizewinrect));

                checksumWidth = sizewin->gamewidth;
                checksumHeight = sizewin->gameheight;

                if (fabs(checksumWidth - self.gameView.frame.size.width) > 1.0) {
                    free(sizewin);
                    break;
                }

                if (fabs(checksumHeight - self.gameView.frame.size.height) > 1.0) {
                    free(sizewin);
                    break;
                }

                x0 = sizewin->x0;
                y0 = sizewin->y0;
                x1 = sizewin->x1;
                y1 = sizewin->y1;
                rect = NSMakeRect(x0, y0, x1 - x0, y1 - y0);
                if (rect.size.width < 0)
                    rect.size.width = 0;
                if (rect.size.height < 0)
                    rect.size.height = 0;
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
                free(sizewin);
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
            if (req->a2 < 0)
                bg = theme.bufferBackground;
            else
                bg = [NSColor colorFromInteger:req->a2];
            if (req->a1 == -1) {
                self.lastAutoBGColor = bg;
                [self setBorderColor:bg];
                changedBorderThisTurn = YES;
            }

            if (reqWin) {
                [reqWin setBgColor:req->a2];
            }
            break;

        case DRAWIMAGE:
            if (reqWin) {
                NSImage *lastimage = self.imageHandler.lastimage;
                if (lastimage && lastimage.size.width > 0 && lastimage.size.height > 0) {
                    struct drawrect *drawstruct = malloc(sizeof(struct drawrect));
                    memcpy(drawstruct, buf, sizeof(struct drawrect));
                    [reqWin drawImage:lastimage
                                 val1:(glsi32)drawstruct->x
                                 val2:(glsi32)drawstruct->y
                                width:drawstruct->width
                               height:drawstruct->height
                                style:drawstruct->style];
                    free(drawstruct);
                }
            }
            break;

        case FILLRECT:
            if (reqWin) {
                [reqWin fillRects:(struct fillrect *)buf count:req->a2];
            }
            break;

        case PRINT:
            self.printsAndClearsThisTurn++;
            if (!self.gwindows.count && shouldRestoreUI) {
                self.windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = self.gwindows[@(req->a1)];
            }
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
            if (!self.gwindows.count && shouldRestoreUI) {
                self.windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = self.gwindows[@(req->a1)];
            }
            ans->cmd = OKAY;
            ans->a1 = 0;
            ans->a2 = 0;
            if (reqWin && req->len) {
                ans->a1 = (int)[self handleUnprintOnWindow:reqWin string:(unichar *)buf length:req->len / sizeof(unichar)];
            }
            break;

        case MOVETO:
            if (reqWin) {
                NSUInteger x = (NSUInteger)req->a2;
                NSUInteger y = (NSUInteger)req->a3;
                [reqWin moveToColumn:x row:y];
            }
            break;

        case SETECHO:
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]])
                [(GlkTextBufferWindow *)reqWin echo:(req->a2 != 0)];
            break;

            /*
             * Request and cancel events
             */

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

#pragma mark Request and cancel events

        case INITLINE:
            [self performScroll];

            if (!self.gwindows.count && shouldRestoreUI) {
                buf = "\0";
                self.windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = self.gwindows[@(req->a1)];
            }
            if (reqWin && self.gameID != kGameIsAColderLight && !skipNextScriptCommand) {
                NSString *preloaded = [NSString stringWithCharacters:(unichar *)buf length:(NSUInteger)req->len / sizeof(unichar)];
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
            break;

        case CANCELLINE:
            ans->cmd = OKAY;
            if (reqWin) {
                NSString *str = reqWin.cancelLine;
                ans->len = str.length * sizeof(unichar);
                if (ans->len > GLKBUFSIZE)
                    ans->len = GLKBUFSIZE;
                [str getCharacters:(unsigned short *)buf
                             range:NSMakeRange(0, str.length)];
            }
            break;

        case INITCHAR:
            if (!self.gwindows.count && shouldRestoreUI) {
                GlkController *g = restoredControllerLate;
                self.windowsToRestore = restoredControllerLate.gwindows.allValues;
                if (g.commandScriptRunning) {
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
                [self restoreUI];
                reqWin = self.gwindows[@(req->a1)];
            }

            // Hack to fix the Level 9 Adrian Mole games.
            // These request and cancel lots of char events every second,
            // which breaks scrolling, as we normally scroll down
            // one screen on every char event.
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
            break;

        case CANCELCHAR:
            if (reqWin)
                [reqWin cancelChar];
            break;

        case INITMOUSE:
            if (!self.gwindows.count && shouldRestoreUI) {
                self.windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = self.gwindows[@(req->a1)];
            }
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
            if (!self.gwindows.count && shouldRestoreUI) {
                self.windowsToRestore = restoredControllerLate.gwindows.allValues;
                [self restoreUI];
                reqWin = self.gwindows[@(req->a1)];
            }
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

#pragma mark Non-standard Glk extensions stuff

        case MAKETRANSPARENT:
            if (reqWin)
                [reqWin makeTransparent];
            break;

        case SETTITLE:
            [self handleChangeTitle:(char *)buf length:req->len];
            break;

        case AUTOSAVE:
            [self handleAutosave:req->a2];
            break;

            // This just kills the interpreter process and restarts it from scratch.
            // Used if an autorestore fails.
        case RESET:
            [self reset:nil];
            break;

            // Used by Tads 3 to adapt the banner width.
        case BANNERCOLS:
            ans->cmd = OKAY;
            ans->a1 = 0;
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]] ) {
                GlkTextBufferWindow *banner = (GlkTextBufferWindow *)reqWin;
                ans->a1 = (int)banner.numberOfColumns;
            }
            break;

            // Used by Tads 3 to adapt the banner height.
        case BANNERLINES:
            ans->cmd = OKAY;
            ans->a1 = 0;
            if (reqWin && [reqWin isKindOfClass:[GlkTextBufferWindow class]] ) {
                GlkTextBufferWindow *banner = (GlkTextBufferWindow *)reqWin;
                ans->a1 = (int)banner.numberOfLines;
            }
            break;

        case PURGEIMG:
            if (req->len) {
                buf[req->len] = 0;
                [self.imageHandler purgeImage:req->a1
                          withReplacement:@(buf)
                                     size:(NSUInteger)req->a2];
            } else {
                [self.imageHandler purgeImage:req->a1
                          withReplacement:nil
                                     size:0];
            }
            break;

        case REFRESH:
//            This updates an existing window on-the-fly with the styles
//            that normally would only be applied to a new window.
//            It can also update any inline images.
            if ([reqWin isKindOfClass:[GlkTextBufferWindow class]]) {
                reqWin.styleHints = [GlkWindow deepCopyOfStyleHintsArray:self.bufferStyleHints];
                if (req->a2 > 0)
                    [((GlkTextBufferWindow *)reqWin) updateImageAttachmentsWithXScale: req->a2 / 1000.0 yScale: req->a3 / 1000.0 ];
            } else if ([reqWin isKindOfClass:[GlkTextGridWindow class]]) {
                reqWin.styleHints = [GlkWindow deepCopyOfStyleHintsArray:self.gridStyleHints];
            } else {
                break;
            }
            [self flushDisplay];
            [reqWin prefsDidChange];
            break;

        case MENUITEM: {
            if (self.gameID == kGameIsJourney) {
                [self.journeyMenuHandler handleMenuItemOfType:(JourneyMenuType)req->a1 column:(NSUInteger)req->a2 line:(NSUInteger)req->a3 stopflag:(req->a4 == 1) text:(char *)buf length:(NSUInteger)req->len];
            } else {
                [self.infocomV6MenuHandler handleMenuItemOfType:(InfocomV6MenuType)req->a1 index:(NSUInteger)req->a2 total:(NSUInteger)req->a3 text:(char *)buf length:(NSUInteger)req->len];
            }
            break;
        }

        default:
            NSLog(@"glkctl: unhandled request (%d)", req->cmd);
            break;
    }

    if (req->cmd != AUTOSAVE)
        lastRequest = req->cmd;
    return NO; /* keep reading */
}

@end

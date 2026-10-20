//
//  CommandScriptHandler.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-11.
//
#include "glk.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkEvent.h"

#import "GlkController.h"
#import "NSString+Categories.h"

#import "CommandScriptHandler.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"
#import "Constants.h"

extern NSArray *gDocFileTypes;
extern NSArray *gSaveFileTypes;

NSNotificationName const GlkCommandScriptDidCompleteNotification = @"GlkCommandScriptDidComplete";

@implementation CommandScriptHandler

+ (BOOL) supportsSecureCoding {
    return YES;
}

// Under XCTest a modal alert stalls the whole run: the main thread sits in the
// modal session, so no test timeout can fire until someone clicks. There the
// problem is logged instead, and the test fails on its own terms.
static BOOL SPRunningUnderTests(void) {
    return NSProcessInfo.processInfo.environment[@"XCTestConfigurationFilePath"] != nil;
}

static void SPReportScriptProblem(NSString *message, NSString *informativeText) {
    if (SPRunningUnderTests()) {
        NSLog(@"CommandScriptHandler: %@ %@", message, informativeText);
        return;
    }
    [[NSAlert alertWithMessage:message info:informativeText buttons:nil] runModal];
}

// Asks the user to confirm, with a "Do not show this message again" box
// whose state is kept under suppressionKey. Returns YES to go ahead.
static BOOL SPConfirmUnlessSuppressed(NSString *suppressionKey, NSString *message, NSString *informativeText) {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults boolForKey:suppressionKey] || SPRunningUnderTests()) {
        NSLog(@"%@ suppressed", suppressionKey);
        return YES;
    }

    NSAlert *anAlert = [NSAlert alertWithMessage:message
                                            info:informativeText
                                         buttons:@[NSLocalizedString(@"Okay", nil), NSLocalizedString(@"Cancel", nil)]];
    [anAlert showSuppressionButtonWithTitle:nil];
    return [anAlert runModalSuppressingWithKey:suppressionKey] != NSAlertSecondButtonReturn;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    self.commandString = [decoder decodeObjectOfClass:[NSString class] forKey:@"commandString"];
    self.commandArray = [decoder decodeObjectOfClass:[NSArray<NSValue *> class] forKey:@"commandArray"];
    _commandIndex = (NSUInteger)[decoder decodeIntegerForKey:@"commandIndex"];
    _lastCommandWindow = [decoder decodeIntegerForKey:@"lastCommandWindow"];
    _lastCommandType = (kLastCommandType)[decoder decodeIntegerForKey:@"lastCommandType"];
    self.untypedCharacters = [decoder decodeObjectOfClass:[NSString class] forKey:@"untypedCharacters"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_commandString forKey:@"commandString"];
    [encoder encodeObject:_commandArray forKey:@"commandArray"];
    // Step back one line so that the command in progress is sent again
    // after an autorestore. (Unsigned: don't step back from 0.)
    [encoder encodeInteger:_commandIndex ? (NSInteger)_commandIndex - 1 : 0 forKey:@"commandIndex"];
    [encoder encodeInteger:_lastCommandWindow forKey:@"lastCommandWindow"];
    [encoder encodeInteger:(NSInteger)_lastCommandType forKey:@"lastCommandType"];
    [encoder encodeObject:_untypedCharacters forKey:@"untypedCharacters"];
}


- (void)startCommandScript:(NSString *)string inWindow:(GlkWindow *)win {
    if (_glkctl.commandScriptRunning)
        return;
    _glkctl.commandScriptRunning = YES;
    _commandString = string;
    _commandArray = [string lineRangesKeepEmptyLines:YES];
    if (_commandArray.count > 10000) {
        NSLog(@"Command script with %ld lines too long, bailing.", _commandArray.count);
        _commandString = nil;
        return;
    }
    _commandIndex = 0;
    if ([win hasLineRequest]) {
        [self sendCommandLineToWindow:win];
    } else if ([win hasCharRequest]) {
        [self sendCommandKeyPressToWindow:win];
    }
}

- (nullable NSString *)nextCommandScriptLine {
    for (;;) {
        if (_commandIndex >= _commandArray.count) {
            // At last command
            _commandString = nil;
            _commandArray = nil;
            _glkctl.commandScriptRunning = NO;

            // Post notification that command script has completed
            [[NSNotificationCenter defaultCenter] postNotificationName:GlkCommandScriptDidCompleteNotification
                                                                object:self
                                                              userInfo:nil];
        }
        if (!_commandString)
            return nil;
        NSString *command = [_commandString substringWithRange:_commandArray[_commandIndex].rangeValue];
        command = [command stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]];
        _commandIndex++;
        if (command.length > 10000) {
            NSLog(@"Command with %ld characters too long, bailing!", command.length);
            _commandString = nil;
            return nil;
        }
        if ([command hasPrefix:@"# "])
            continue;
        return command;
    }
}

- (void)sendCommandLineToWindow:(GlkWindow *)win {
    if (_untypedCharacters.length > 1) {
        _commandIndex--;
    }

    _untypedCharacters = nil;
    NSString *command = [self nextCommandScriptLine];
    if (command) {
        [win sendCommandLine:command];
        _lastCommandWindow = win.name;
        _lastCommandType = kCommandTypeLine;
    }
}

- (void)sendCommandKeyPressToWindow:(GlkWindow *)win {
    if (!_untypedCharacters.length) {
        NSString *command = [self nextCommandScriptLine];
        if (command.length == 1)
            _untypedCharacters = command;
        else
            _untypedCharacters = [command stringByAppendingString:@"\n"];
    }

    // If the command is a single character on a line,
    // we send it as a single key press. Otherwise we
    // send the characters one by one and end with
    // a newline character, to support games that roll their own
    // line input using key requests, such as Bureaucracy.

    // If you want make sure a single character is sent
    // as a command with a newline at the end, either put a space
    // before or after it, so that the line is not 1 character long,
    // or just put a blank line after it, which will be sent at a single
    // newline character.

    unsigned keyPress = keycode_Return;
    if (_untypedCharacters.length)
        keyPress = [_untypedCharacters characterAtIndex:0];
    if (keyPress == '\n' || keyPress == '\r') {
        keyPress = keycode_Return;
        if (_untypedCharacters.length)
            _untypedCharacters = @"";
    } else if (_glkctl.gameID == kGameIsJourney) {
        keyPress = chartokeycode(keyPress);
    }

    [win sendKeypress:keyPress];
    if (_untypedCharacters.length > 1)
        _untypedCharacters = [_untypedCharacters substringFromIndex:1];
    else
        _untypedCharacters = nil;
    _lastCommandWindow = win.name;
    _lastCommandType = kCommandTypeChar;
}

- (BOOL)commandScriptInPasteboard:(NSPasteboard *)pboard fromWindow:(GlkWindow *)gwin {
    NSString *string = nil;

    if ( [pboard.types containsObject:NSPasteboardTypeString] ) {
        string = [pboard stringForType:NSPasteboardTypeString];
    } else if ( [pboard.types containsObject:NSPasteboardTypeURL] ) {
        NSURL *fileURL = [NSURL URLFromPasteboard:pboard];
        if ([gDocFileTypes indexOfObject:fileURL.pathExtension.lowercaseString] != NSNotFound) {
            [self runCommandsFromFile:fileURL.path inWindow:gwin];
            return YES;
        }
        if ([gSaveFileTypes indexOfObject:fileURL.pathExtension.lowercaseString] != NSNotFound) {
            [self restoreFromSaveFile:fileURL.path];
            return YES;
        }
    }

    if (string.length) {
        [self startCommandScript:string inWindow:gwin];
        return YES;
    } else {
        return NO;
    }
}

- (void)runCommandsFromFile:(NSString *)filename {
    [self runCommandsFromFile:filename inWindow:nil];
}

- (void)runCommandsFromFile:(NSString *)filename inWindow:(nullable GlkWindow *)win {
    NSURL *fileURL = [NSURL fileURLWithPath:filename isDirectory:NO];
    NSError *error = nil;
    NSAttributedString *attStr = [[NSAttributedString alloc] initWithURL:fileURL options:@{} documentAttributes:nil error:&error];
    NSString *string = attStr.string;
    if (error || !string.length) {
        SPReportScriptProblem(NSLocalizedString(@"Could not read command script.", nil),
                              [NSString stringWithFormat:NSLocalizedString(@"\"%@\" is not a valid text file", nil), filename.lastPathComponent]);
        return;
    }

    if (!win)
        win = [self findGlkWindowWithInput];
    if (!win) {
        SPReportScriptProblem(NSLocalizedString(@"No prompt.", nil),
                              NSLocalizedString(@"Cannot find an input prompt for this command script.", nil));
        return;
    }

    if (!SPConfirmUnlessSuppressed(kCommandScriptAlertSuppressionKey,
                                   NSLocalizedString(@"Run command script?", nil),
                                   [NSString stringWithFormat:NSLocalizedString(@"Do you want to run the command script %@ with this game?", nil), filename.lastPathComponent]))
        return;

    [self startCommandScript:string inWindow:win];
}

- (nullable GlkWindow *)windowWithLineRequest {
    for (GlkWindow *win in _glkctl.gwindows.allValues) {
        if ([win hasLineRequest])
            return win;
    }
    return nil;
}

// A window with a line request, or failing that one with a char request.
- (nullable GlkWindow *)findGlkWindowWithInput {
    GlkWindow *foundWin = [self windowWithLineRequest];
    if (foundWin)
        return foundWin;
    for (GlkWindow *win in _glkctl.gwindows.allValues) {
        if ([win hasCharRequest])
            return win;
    }
    return nil;
}

- (void)restoreFromSaveFile:(NSString *)filename {
    GlkWindow *win = [self windowWithLineRequest];
    if (!win) {
        SPReportScriptProblem(NSLocalizedString(@"No prompt.", nil),
                              NSLocalizedString(@"Cannot find an input prompt to restore this save from.", nil));
        return;
    }
    if (!SPConfirmUnlessSuppressed(kSaveFileAlertSuppressionKey,
                                   NSLocalizedString(@"Restore from save file?", nil),
                                   [NSString stringWithFormat:NSLocalizedString(@"Do you want to restore this game from the save file %@?", nil), filename.lastPathComponent]))
        return;
    _glkctl.pendingSaveFilePath = filename;
    [win sendCommandLine:@"restore"];
}

- (void)copyPropertiesFrom:(CommandScriptHandler *)handler {
    _commandIndex = handler.commandIndex;
    _commandString = [handler.commandString copy];
    _commandArray = [handler.commandArray copy];
    _lastCommandWindow = handler.lastCommandWindow;
    _lastCommandType = handler.lastCommandType;
    _untypedCharacters = [handler.untypedCharacters copy];
    if (_untypedCharacters.length)
        _lastCommandType = kCommandTypeChar;
    _glkctl.commandScriptRunning = YES;
}

@end

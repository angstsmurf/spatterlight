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

extern NSArray *gDocFileTypes;
extern NSArray *gSaveFileTypes;

@implementation CommandScriptHandler

+ (BOOL) supportsSecureCoding {
    return YES;
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
    [encoder encodeInteger:(NSInteger)_commandIndex - 1 forKey:@"commandIndex"];
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
    if (_commandIndex >= _commandArray.count) {
        // At last command
        _commandString = nil;
        _commandArray = nil;
        _glkctl.commandScriptRunning = NO;
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
    return command;
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
        string = [pboard  stringForType:NSPasteboardTypeString];
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
    NSURL *fileURL = [NSURL fileURLWithPath:filename  isDirectory:NO];
    NSError *error = nil;
    NSAttributedString *attStr = [[NSAttributedString alloc] initWithURL:fileURL options:@{} documentAttributes:nil error:&error];
    NSString *string = attStr.string;
    if (error || !string.length) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Could not read command script.", nil);
        alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"\"%@\" is not a valid text file", nil), filename.lastPathComponent];
        [alert runModal];
        return;
    }

    if (!win)
        win = [self findGlkWindowWithInput];
    if (!win) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"No prompt.", nil);
        alert.informativeText = NSLocalizedString(@"Cannot find an input prompt for this command script.", nil);
        [alert runModal];
        return;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults boolForKey:@"CommandScriptAlertSuppression"]) {
        NSLog(@"Command script alert suppressed");
    } else {
        NSAlert *anAlert = [[NSAlert alloc] init];
        anAlert.messageText = NSLocalizedString(@"Run command script?", nil);

        anAlert.informativeText = [NSString stringWithFormat:@"Do you want to run the command script %@ with this game?", filename.lastPathComponent];
        anAlert.showsSuppressionButton = YES; // Uses default checkbox title

        [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSInteger choice = [anAlert runModal];

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:@"CommandScriptAlertSuppression"];
        }
        if (choice == NSAlertSecondButtonReturn) {
            return;
        }
    }

    [self startCommandScript:string inWindow:win];
}

- (nullable GlkWindow *)findGlkWindowWithInput {
    GlkWindow *foundWin = nil;
    for (GlkWindow *win in _glkctl.gwindows.allValues) {
        if ([win hasLineRequest]) {
            foundWin = win;
            break;
        }
    }
    if (!foundWin) {
        // Found no window with line input request
        for (GlkWindow *win in _glkctl.gwindows.allValues) {
            if ([win hasCharRequest]) {
                foundWin = win;
                // Found window with char input request
                break;
            }
        }
    }
    return foundWin;
}

- (void)restoreFromSaveFile:(NSString *)filename {
    GlkWindow *win = nil;
    for (GlkWindow *foundwin in _glkctl.gwindows.allValues) {
        if ([foundwin hasLineRequest]) {
            win = foundwin;
            break;
        }
    }
    if (!win) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"No prompt.", nil);
        alert.informativeText = NSLocalizedString(@"Cannot find an input prompt to restore this save from.", nil);
        [alert runModal];
        return;
    }
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if ([defaults boolForKey:@"SaveFileAlertSuppression"]) {
        NSLog(@"Save file alert suppressed");
    } else {
        NSAlert *anAlert = [[NSAlert alloc] init];
        anAlert.messageText = NSLocalizedString(@"Restore from save file?", nil);

        anAlert.informativeText = [NSString stringWithFormat:@"Do you want to restore this game from the save file %@?", filename.lastPathComponent];
        anAlert.showsSuppressionButton = YES; // Uses default checkbox title

        [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSInteger choice = [anAlert runModal];

        if (anAlert.suppressionButton.state == NSOnState) {
            // Suppress this alert from now on
            [defaults setBool:YES forKey:@"SaveFileAlertSuppression"];
        }
        if (choice == NSAlertSecondButtonReturn) {
            return;
        }
    }
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

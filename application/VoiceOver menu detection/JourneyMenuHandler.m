//
//  JourneyMenuHandler.m
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "AppDelegate.h"
#import "GlkController.h"
#import "GlkTextGridWindow.h"
#import "GlkEvent.h"
#import "InputTextField.h"
#import "JourneyMenuHandler.h"

@interface JourneyMenuItem : NSObject

@property NSString *title;
@property NSUInteger column;
@property NSUInteger line;
@property NSInteger tag;

@property NSString *actor;

- (instancetype)initWithTitle:(NSString *)title column:(NSUInteger)column line:(NSUInteger)line tag:(NSInteger)tag;

@end

@implementation JourneyMenuItem

- (instancetype)initWithTitle:(NSString *)title column:(NSUInteger)column line:(NSUInteger)line tag:(NSInteger)tag {
    self = [super init];
    if (self) {
        self.title = title;
        self.column = column;
        self.line = line;
        self.tag = tag;
    }
    return self;
}

@end

@interface JourneyTextFormatter : MyTextFormatter

- (instancetype)initWithMaxLength:(NSUInteger)maxLength elvish:(BOOL)elvish;

@property BOOL elvish;

@end

@implementation JourneyTextFormatter

- (instancetype)initWithMaxLength:(NSUInteger)maxLength elvish:(BOOL)elvish {
    if (self = [super initWithMaxLength:maxLength]) {
        self.elvish = elvish;
    }
    return self;
}

- (BOOL)isBadCharacter:(unichar)c {
    if (_elvish) {
        switch (c) {
            case ' ':
            case '-':
            case '\'':
            case '.':
            case ',':
            case '?':
                return NO;
            default:
                break;
        }
    }
    return !isalpha(c);
}

- (BOOL)isPartialStringValid:(NSString * __autoreleasing *)partialStringPtr
proposedSelectedRange:(NSRangePointer)proposedSelRangePtr
originalString:(NSString *)origString
originalSelectedRange:(NSRange)origSelRange
errorDescription:(NSString * __autoreleasing *)error
{
    BOOL valid = [super isPartialStringValid:partialStringPtr proposedSelectedRange:proposedSelRangePtr originalString:origString originalSelectedRange:origSelRange errorDescription:error];

    NSString *proposedString = *partialStringPtr;
    NSMutableSet<NSString *> *badCharacters = [NSMutableSet new];
    for (NSUInteger i = 0; i < proposedString.length; i++) {
        unichar c = [proposedString characterAtIndex:i];
        if ([self isBadCharacter:c]) {
            [badCharacters addObject:[NSString stringWithCharacters:&c length:1]];
            valid = NO;
        }
    }

    for (NSString *bad in badCharacters) {
        proposedString = [proposedString stringByReplacingOccurrencesOfString:bad withString:@""];
    }

    if (!_elvish && proposedString.length > 0) {
        unichar first = [proposedString characterAtIndex:0];
        if (islower(first)) {
            valid = NO;
            first = (unichar)toupper(first);
            proposedString = [NSString stringWithFormat:@"%@%@", [NSString stringWithCharacters:&first length:1], [proposedString substringFromIndex:1]];
        }
        badCharacters = [NSMutableSet new];
        for (NSUInteger i = 1; i < proposedString.length; i++) {
            unichar c = [proposedString characterAtIndex:i];
            if (isupper(c)) {
                [badCharacters addObject:[NSString stringWithCharacters:&c length:1]];
                valid = NO;
            }
        }
        for (NSString *bad in badCharacters) {
            unichar lower = (unichar)tolower([bad characterAtIndex:0]);
            proposedString = [proposedString stringByReplacingOccurrencesOfString:bad withString:[NSString stringWithCharacters:&lower length:1]];
        }
    }
    *partialStringPtr = proposedString;

    return valid;
}


@end

@interface JourneyMenuHandler () {
    BOOL shouldStartNewJourneyPartyMenu;
    BOOL shouldStartNewJourneyMembersMenu;
    BOOL shouldStartNewJourneyDialogMenu;
    BOOL skipNextDialog;
    NSMutableArray<JourneyMenuItem *> *journeyPartyMenuItems;
    NSMutableArray<JourneyMenuItem *> *journeyLastPartyMenuItems;
    NSMutableArray<JourneyMenuItem *> *journeyVerbMenuItems;
    NSMutableArray<JourneyMenuItem *> *journeyGlueStrings;
    NSInteger tagCounter;
}
@end

@implementation JourneyMenuHandler

- (instancetype)initWithDelegate:(GlkController *)delegate gridWindow:(GlkTextGridWindow *)gridwindow {
    self = [super init];
    if (self) {
        shouldStartNewJourneyPartyMenu = YES;
        shouldStartNewJourneyMembersMenu = YES;
        shouldStartNewJourneyDialogMenu = YES;
        self.delegate = delegate;
        self.textGridWindow = gridwindow;
    }
    return self;
}

- (void)journeyPartyAction:(id)sender {
    NSMenuItem *senderItem = (NSMenuItem *)sender;
    for (JourneyMenuItem *item in journeyPartyMenuItems)
        if (item.tag == senderItem.tag) {
            [self sendCorrespondingMouseEventForMenuItem:item];
            break;
        }
}

- (void)journeyMemberDummyAction:(id)sender {}

- (void)journeyMemberVerbAction:(id)sender {
    NSMenuItem *senderItem = (NSMenuItem *)sender;
    for (JourneyMenuItem *item in journeyVerbMenuItems)
        if (item.tag == senderItem.tag) {
            [self sendCorrespondingMouseEventForMenuItem:item];
            break;
        }
}


- (void)sendCorrespondingMouseEventForMenuItem:(JourneyMenuItem *)item {
    NSUInteger x = 0;
    NSSize winsize = [_textGridWindow currentSizeInChars];

    if (item.column < 1) {
        x = 2;
    } else {
        NSUInteger commandWidth = (NSUInteger)winsize.width / 5;
        NSUInteger nameWidth = (NSUInteger)winsize.width % 5;

        x = nameWidth + commandWidth * (item.column) + 2;
    }

    NSUInteger y = (NSUInteger)winsize.height - 5 + item.line;
    
    // Check if we are in border mode (i.e. if interpreter is set to Amiga)
    unichar c = [_textGridWindow characterAtPoint:NSZeroPoint];
    if (c == '/') {
        // We seem to have a border
        y--;
    }

    NSPoint point = NSMakePoint(x, y);

    NSLog(@"Simulated mouse click at %@", NSStringFromPoint(point));
//    [self printCharactersAtPoints:point];
    [self.delegate markLastSeen];
    GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:point forWindow:_textGridWindow.name];
    [self.delegate queueEvent:gev];
}

- (void)displayAlertWithTextEntry:(NSString *)messageText elvish:(BOOL)elvish {
    if (!self.delegate.voiceOverActive) {
        [self deleteAllJourneyMenus];
        return;
    }
    NSView *accessoryView = [self textEntryAccessoryViewElvish:elvish];

    NSAlert *journeyDialog = [NSAlert new];

    journeyDialog.accessoryView = accessoryView;
    journeyDialog.messageText = NSLocalizedString(messageText, nil);

    JourneyMenuHandler __weak *weakSelf = self;

    [journeyDialog beginSheetModalForWindow:self.delegate.window completionHandler:^(NSInteger result) {
            NSString *enteredText = weakSelf.journeyTextField.stringValue;
            size_t length = enteredText.length;
            if (length > 40)
                length = 40;
            for (NSUInteger i = 0; i < length; i++) {
                unsigned keyPress = [enteredText characterAtIndex:i];
                keyPress = chartokeycode(keyPress);
                [self.textGridWindow sendKeypress:keyPress];
            }
            [self.textGridWindow sendKeypress:keycode_Return];
            [weakSelf.delegate markLastSeen];
    }];
}

- (void)displayAlertWithText:(NSString *)messageText {
    if (!self.delegate.voiceOverActive) {
        [self deleteAllJourneyMenus];
        return;
    }
    NSAlert *journeyDialog = [NSAlert new];

    journeyDialog.messageText = messageText;

    [journeyDialog addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [journeyDialog addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    JourneyMenuHandler __weak *weakSelf = self;

    [journeyDialog beginSheetModalForWindow:self.delegate.window completionHandler:^(NSInteger result){
        if (result == NSAlertFirstButtonReturn) {
            [weakSelf sendCorrespondingMouseEventForMenuItem:weakSelf.journeyDialogMenuItems.firstObject];
        } else {
            [weakSelf sendCorrespondingMouseEventForMenuItem: weakSelf.journeyDialogMenuItems.lastObject];
        }
    }];
}

- (void)displayAlertMenuWithMessageText:(NSString *)messageText {
    if (!self.delegate.voiceOverActive) {
        [self deleteAllJourneyMenus];
        return;
    }
    NSMutableArray *strings = [NSMutableArray new];
    for (JourneyMenuItem *dialogMenuItem in _journeyDialogMenuItems) {
        if (![dialogMenuItem.title isEqualToString:@"[cancel]"])
            [strings addObject:dialogMenuItem.title];
    }
    NSView *accessoryView = [self objectMenuAccessoryViewWithStrings:strings];

    NSAlert *journeyDialog = [NSAlert new];

//    journeyDialog.alertStyle = NSAlertStyleWarning;
    journeyDialog.accessoryView = accessoryView;

    journeyDialog.messageText = NSLocalizedString(messageText, nil);
    [journeyDialog addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [journeyDialog addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    JourneyMenuHandler __weak *weakSelf = self;

    [journeyDialog beginSheetModalForWindow:self.delegate.window completionHandler:^(NSInteger result){
        if (result == NSAlertFirstButtonReturn) {
            NSMenuItem *selectedItem = weakSelf.journeyPopupButton.selectedItem;
            for (JourneyMenuItem *dialogMenuItem in weakSelf.journeyDialogMenuItems) {
                if ([dialogMenuItem.title isEqualToString:selectedItem.title]) {
                    [weakSelf sendCorrespondingMouseEventForMenuItem:dialogMenuItem];
                    break;
                }
            }
        } else {
            [weakSelf sendCorrespondingMouseEventForMenuItem: weakSelf.journeyDialogMenuItems.lastObject];
        }
    }];
}

- (NSView *)textEntryAccessoryViewElvish:(BOOL)elvish {

    NSView  *accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 100, 32.0)];

    _journeyTextField = [[NSTextField alloc] initWithFrame:NSMakeRect(0.0, 10.0, 100, 22.0)];

    JourneyTextFormatter *formatter = [[JourneyTextFormatter alloc] initWithMaxLength:(elvish ? 13 : 8) elvish:elvish];

    _journeyTextField.formatter = formatter;

    [accessoryView addSubview:_journeyTextField];

    return accessoryView;
}

- (NSView *)objectMenuAccessoryViewWithStrings:(NSArray<NSString *> *)buttonItems {

    NSView  *accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 100, 32.0)];

    _journeyPopupButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0.0, 10.0, 100, 22.0) pullsDown:NO];
    [_journeyPopupButton addItemsWithTitles:buttonItems];

    [accessoryView addSubview:_journeyPopupButton];

    return accessoryView;
}

- (BOOL)partyIsEqualToLast {
    if (journeyLastPartyMenuItems.count == 0 || journeyLastPartyMenuItems.count != journeyPartyMenuItems.count)
        return NO;
    for (NSUInteger i = 0; i < journeyLastPartyMenuItems.count; i++) {
        if (![journeyLastPartyMenuItems[i].title isEqualToString:journeyPartyMenuItems[i].title]) {
            return NO;
        }
    }
    return YES;
}

- (void)deleteAllJourneyMenus {
    NSMenuItem *journeyMembersMenu = ((AppDelegate*)NSApplication.sharedApplication.delegate).journeyIndividualCommandsMenuItem;
    NSMenuItem *journeyPartyMenu = ((AppDelegate*)NSApplication.sharedApplication.delegate).journeyPartyMenuItem;

    journeyMembersMenu.hidden = YES;
    journeyMembersMenu.enabled = NO;
    [journeyMembersMenu.submenu removeAllItems];
    journeyPartyMenu.hidden = YES;
    journeyPartyMenu.enabled = NO;
    [journeyPartyMenu.submenu removeAllItems];
    journeyVerbMenuItems = [NSMutableArray new];
    journeyGlueStrings = [NSMutableArray new];
    journeyPartyMenuItems = [NSMutableArray new];
    journeyLastPartyMenuItems = [NSMutableArray new];
    _journeyDialogMenuItems = [NSMutableArray new];
    shouldStartNewJourneyPartyMenu = YES;
    shouldStartNewJourneyDialogMenu = YES;
    shouldStartNewJourneyMembersMenu = YES;
    skipNextDialog = YES;
}

- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *)buf length:(NSUInteger)len {

   if (type == kJMenuTypeTextEntry) {
        BOOL elvish = stopflag;
        NSString *messageText;
        if (elvish) {
            if (journeyVerbMenuItems.count == 0)
                return;
            for (JourneyMenuItem *actor in journeyVerbMenuItems) {
                if (actor.line == line) {
                    messageText = actor.actor;
                    break;
                }
            }
            if (messageText == nil)
                return;
            messageText = [messageText stringByAppendingString:@" says..."];
        } else {
            messageText = @"Please enter a new name for the main character:";
        }
        [self displayAlertWithTextEntry:messageText elvish:elvish];
        return;
    }

    NSMenuItem *journeyMembersMenu = ((AppDelegate*)NSApplication.sharedApplication.delegate).journeyIndividualCommandsMenuItem;
    NSMenuItem *journeyPartyMenu = ((AppDelegate*)NSApplication.sharedApplication.delegate).journeyPartyMenuItem;

    if (type == kJMenuTypeDeleteAll) {
        [self deleteAllJourneyMenus];
        return;
    }

    unichar cstring[20];
    for (NSUInteger i = 0; i < len; i++) {
        cstring[i] = (unichar)buf[i];
    }
    NSString *str = [NSString stringWithCharacters:cstring length:len];
    if (!str) {
        NSLog(@"ERROR!");
        return;
    }

    NSLog(@"Received menu item with text %@ len %ld", str, len);

    JourneyMenuItem *item = [[JourneyMenuItem alloc] initWithTitle:str column:column line:line tag:tagCounter++];

    if (type == kJMenuTypeParty) {
        if (journeyPartyMenu == nil) {
            NSLog(@"Error!");
            return;
        }

        if (_delegate.voiceOverActive) {
            journeyPartyMenu.hidden = NO;
            journeyPartyMenu.enabled = YES;
        }

        if (shouldStartNewJourneyPartyMenu) {
            [journeyPartyMenu.submenu removeAllItems];
            journeyPartyMenuItems = [NSMutableArray new];
        }

        [journeyPartyMenuItems addObject:item];

        shouldStartNewJourneyPartyMenu = stopflag;
        if (stopflag) {
            if ([item.title isEqualToString:@"[cancel]"]) {
                if (!self.delegate.voiceOverActive) {
                    [self deleteAllJourneyMenus];
                    return;
                }
                if ([self partyIsEqualToLast]) {
                    shouldStartNewJourneyPartyMenu = YES;
                    return;
                }
                _journeyDialogMenuItems = journeyPartyMenuItems;
                journeyLastPartyMenuItems = journeyPartyMenuItems;
                if (skipNextDialog) {
                    skipNextDialog = NO;
                    return;
                }
                [self displayAlertMenuWithMessageText:@"Please Select One:"];
                return;
            } else {
                skipNextDialog = NO;
                journeyLastPartyMenuItems = [NSMutableArray new];
                for (JourneyMenuItem *menuToAdd in journeyPartyMenuItems) {
                    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:menuToAdd.title action:@selector(journeyPartyAction:) keyEquivalent:@""];
                    menuItem.tag = menuToAdd.tag;
                    [journeyPartyMenu.submenu addItem:menuItem];
                }
            }
        }
    } else if (type == kJMenuTypeMembers) {
        if (journeyMembersMenu == nil) {
            NSLog(@"Error!");
            return;
        }

        if (_delegate.voiceOverActive) {
            journeyMembersMenu.hidden = NO;
            journeyMembersMenu.enabled = YES;
        }

        if (shouldStartNewJourneyMembersMenu) {
            [journeyMembersMenu.submenu removeAllItems];
            journeyVerbMenuItems = [NSMutableArray new];
        }
        [journeyMembersMenu.submenu addItem:[[NSMenuItem alloc] initWithTitle:str action:@selector(journeyMemberDummyAction:) keyEquivalent:@""]];
        shouldStartNewJourneyMembersMenu = stopflag;
    } else if (type == kJMenuTypeVerbs) {
        if (journeyMembersMenu) {
            NSMenuItem *last = journeyMembersMenu.submenu.itemArray.lastObject;
            last.action = @selector(journeyPartyAction:);
            if (!last.submenu)
                last.submenu = [[NSMenu alloc] initWithTitle:@""];
            NSMenuItem *menuItemToAdd = [[NSMenuItem alloc] initWithTitle:str action:@selector(journeyMemberVerbAction:) keyEquivalent:@""];
            menuItemToAdd.tag = item.tag;
            if ([menuItemToAdd.title characterAtIndex:0] == '[')
                menuItemToAdd.action = nil;
            [last.submenu addItem:menuItemToAdd];
            item.actor = last.title;
            [journeyVerbMenuItems addObject:item];
        }
    } else if (type == kJMenuTypeGlue) {
        NSLog(@"It is a glue string (%@) for line %ld", str, item.line);
        if (shouldStartNewJourneyDialogMenu) {
            journeyGlueStrings = [NSMutableArray new];
            _journeyDialogMenuItems = [NSMutableArray new];
            shouldStartNewJourneyDialogMenu = NO;
        }
        [journeyGlueStrings addObject:item];
    } else if (type == kJMenuTypeObjects) {
        NSLog(@"It is for an object menu");
        [_journeyDialogMenuItems addObject:item];
        shouldStartNewJourneyDialogMenu = stopflag;
        if (stopflag) {
            if (journeyMembersMenu == nil)
                return;

            if (journeyGlueStrings.count == 0)
                return;

            NSString *messageText = nil;
            NSUInteger actorline = journeyGlueStrings.firstObject.line;
            if (journeyVerbMenuItems.count == 0)
                return;
            for (JourneyMenuItem *actor in journeyVerbMenuItems) {
                if (actor.line == actorline) {
                    messageText = actor.actor;
                    break;
                }
            }
            if (messageText == nil)
                return;

            messageText = [messageText stringByAppendingString:@","];

            for (JourneyMenuItem *glueString in journeyGlueStrings) {
                messageText = [messageText stringByAppendingString:@" "];
                messageText = [messageText stringByAppendingString:glueString.title];
            }

            if (_journeyDialogMenuItems.count <= 2) {
                messageText = [messageText stringByAppendingString:@" "];
                messageText = [messageText stringByAppendingString:_journeyDialogMenuItems.firstObject.title];
                [self displayAlertWithText:messageText];
            } else {
                messageText = [messageText stringByAppendingString:@":"];
                [self displayAlertMenuWithMessageText:messageText];
            }
        }
    }
}

@end

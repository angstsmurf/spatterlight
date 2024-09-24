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

+ (BOOL) supportsSecureCoding {
    return YES;
}

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

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        self.title = [decoder decodeObjectOfClass:[NSString class] forKey:@"title"];
        self.column = (NSUInteger)[decoder decodeIntegerForKey:@"column"];
        self.line = (NSUInteger)[decoder decodeIntegerForKey:@"line"];;
        self.tag = [decoder decodeIntegerForKey:@"tag"];
        self.actor = [decoder decodeObjectOfClass:[NSString class] forKey:@"actor"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_title forKey:@"title"];
    [encoder encodeInteger:(NSInteger)_column forKey:@"column"];
    [encoder encodeInteger:(NSInteger)_line forKey:@"line"];
    [encoder encodeInteger:_tag forKey:@"tag"];
    [encoder encodeObject:_actor forKey:@"actor"];
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
    NSInteger tagCounter;
}
@end

@implementation JourneyMenuHandler

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithDelegate:(GlkController *)delegate gridWindow:(GlkTextGridWindow *)gridwindow {
    self = [super init];
    if (self) {
        shouldStartNewJourneyPartyMenu = YES;
        shouldStartNewJourneyMembersMenu = YES;
        shouldStartNewJourneyDialogMenu = YES;
        self.delegate = delegate;
        self.textGridWindow = gridwindow;
        self.journeyDialogClosedTimestamp = [NSDate distantPast];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        self.journeyDialogMenuItems = [decoder decodeObjectOfClass:[NSDictionary class] forKey:@"journeyDialogMenuItems"];
        self.journeyVerbMenuItems = [decoder decodeObjectOfClass:[NSDictionary class] forKey:@"journeyVerbMenuItems"];
        self.journeyPartyMenuItems = [decoder decodeObjectOfClass:[NSArray class] forKey:@"journeyPartyMenuItems"];
        self.journeyLastPartyMenuItems = [decoder decodeObjectOfClass:[NSArray class] forKey:@"journeyLastPartyMenuItems"];
        self.journeyGlueStrings = [decoder decodeObjectOfClass:[NSArray class] forKey:@"journeyGlueStrings"];
        self.capturedMembersMenu = [decoder decodeObjectOfClass:[NSArray class] forKey:@"capturedMembersMenu"];
        self.gridTextWinName = [decoder decodeIntegerForKey:@"textGridWinName"];
        self.shouldShowDialog = [decoder decodeBoolForKey:@"shouldShowDialog"];
        _restoredShowingDialog = self.shouldShowDialog;
        self.storedDialogType = (kJourneyDialogType)[decoder decodeIntegerForKey:@"storedDialogType"];
        self.storedDialogText = [decoder decodeObjectOfClass:[NSString class] forKey:@"storedDialogText"];
        self->shouldStartNewJourneyDialogMenu = [decoder decodeBoolForKey:@"shouldStartNewJourneyDialogMenu"];
        self.journeyDialogClosedTimestamp = [NSDate distantPast];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:_journeyDialogMenuItems forKey:@"journeyDialogMenuItems"];

    [encoder encodeObject:_journeyVerbMenuItems forKey:@"journeyVerbMenuItems"];
    [encoder encodeObject:_journeyPartyMenuItems forKey:@"journeyPartyMenuItems"];
    [encoder encodeObject:_journeyLastPartyMenuItems forKey:@"journeyLastPartyMenuItems"];

    [encoder encodeObject:_journeyGlueStrings forKey:@"journeyGlueStrings"];
    [encoder encodeObject:_capturedMembersMenu forKey:@"capturedMembersMenu"];

    [encoder encodeInteger:_textGridWindow.name forKey:@"textGridWinName"];

    [encoder encodeBool:shouldStartNewJourneyDialogMenu forKey:@"shouldStartNewJourneyDialogMenu"];
    [encoder encodeBool:_shouldShowDialog forKey:@"shouldShowDialog"];

    [encoder encodeInteger:_storedDialogType forKey:@"storedDialogType"];
    [encoder encodeObject:_storedDialogText forKey:@"storedDialogText"];

}

- (NSMenuItem *)journeyPartyMenu {
    if (_journeyPartyMenu == nil) {
        _journeyPartyMenu = ((AppDelegate *)NSApplication.sharedApplication.delegate).journeyPartyMenuItem;
    }
    return _journeyPartyMenu;
}

- (NSMenuItem *)journeyMembersMenu {
    if (_journeyMembersMenu == nil) {
        _journeyMembersMenu = ((AppDelegate *)NSApplication.sharedApplication.delegate).journeyIndividualCommandsMenuItem;
    }
    return _journeyMembersMenu;
}

- (BOOL)membersMenuMatchesArray {
    NSArray<NSMenuItem *> *itemArray = self.journeyMembersMenu.submenu.itemArray;
    if (itemArray.count != _capturedMembersMenu.count)
        return NO;

    for (NSUInteger i = 0; i < itemArray.count; i++) {
        if (![itemArray[i].title isEqualToString:_capturedMembersMenu[i].title])
            return NO;

        NSArray<NSMenuItem *> *subItemArray = itemArray[i].submenu.itemArray;
        NSArray<NSMenuItem *> *submenu = _capturedMembersMenu[i].submenu.itemArray;
        if (subItemArray.count != submenu.count)
            return NO;
        for (NSUInteger j = 0; j < subItemArray.count; j++) {
            if (![submenu[j].title isEqualToString:subItemArray[j].title]) {
                return NO;
            }
        }

    }
    return YES;
}

- (void)recreateJourneyMenus {
    if (![self partyIsEqualToLast] && _journeyPartyMenuItems.count) {
        shouldStartNewJourneyPartyMenu = YES;
        [self.journeyPartyMenu.submenu removeAllItems];
        for (JourneyMenuItem *item in _journeyPartyMenuItems) {
            [self.journeyPartyMenu.submenu addItem:[[NSMenuItem alloc] initWithTitle:item.title action:@selector(journeyPartyAction:) keyEquivalent:@""]];
        }
    }

    if (![self membersMenuMatchesArray] && _capturedMembersMenu.count) {
        [self.journeyMembersMenu.submenu removeAllItems];
        for (NSMenuItem *item in _capturedMembersMenu)
            [_journeyMembersMenu.submenu addItem:item];
    }

    [self showJourneyMenus];

    shouldStartNewJourneyMembersMenu = YES;
    shouldStartNewJourneyPartyMenu = YES;
}

- (void)journeyPartyAction:(id)sender {
    NSMenuItem *senderItem = (NSMenuItem *)sender;
    for (JourneyMenuItem *item in _journeyPartyMenuItems) {
        if ([item.title isEqualToString:senderItem.title]) {
            [self sendCorrespondingMouseEventForMenuItem:item];
            return;
        }
    }
}

- (void)journeyMemberDummyAction:(id)sender {}

- (void)journeyMemberVerbAction:(id)sender {
    NSMenuItem *senderItem = (NSMenuItem *)sender;
    JourneyMenuItem *item = _journeyVerbMenuItems[@(senderItem.tag)];
    if (!item)
        NSLog(@"Error! Verb menu item %@ not found!", senderItem.title);
    else
        [self sendCorrespondingMouseEventForMenuItem:item];
}


- (void)sendCorrespondingMouseEventForMenuItem:(JourneyMenuItem *)item {
    _restoredShowingDialog = NO;

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

    if (x > winsize.width || y > winsize.height) {
        NSLog(@"Mouse click out of bounds at %@", NSStringFromPoint(point));
    }

    [_delegate markLastSeen];
    GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:point forWindow:_textGridWindow.name];
    [self showJourneyMenus];
    [self.delegate queueEvent:gev];
    _shouldShowDialog = NO;
}

- (nullable NSAlert *)journeyAlertWithText:(NSString *)text {
    if (_shouldShowDialog || _reallyShowingDialog)
        return nil;

    _skipNextDialog = NO;

    _shouldShowDialog = YES;
    _storedDialogText = text;

    if (!self.delegate.voiceOverActive) {
        [self hideJourneyMenus];
        return nil;
    }

    NSAlert *journeyDialog = [NSAlert new];

    journeyDialog.messageText = NSLocalizedString(text, nil);
    _reallyShowingDialog = YES;
    return journeyDialog;
}


// Shows modal dialog with text field and OK button.
// The "elvish" flag is a shorthand for all text entry
// that is not player name change
- (void)displayAlertWithTextEntry:(NSString *)messageText elvish:(BOOL)elvish {
    if (elvish)
        _storedDialogType = kJourneyDialogTextEntryElvish;
    else
        _storedDialogType = kJourneyDialogTextEntry;

    NSAlert *journeyDialog = [self journeyAlertWithText:messageText];
    if (!journeyDialog)
        return;

    journeyDialog.accessoryView = [self textEntryAccessoryViewElvish:elvish];

    JourneyMenuHandler __weak *weakSelf = self;
    journeyDialog.window.initialFirstResponder = _journeyTextField;

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
        [self dialogEnded];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            NSAccessibilityPostNotificationWithUserInfo(
                                                        NSApp.mainWindow,
                                                        NSAccessibilityAnnouncementRequestedNotification,
                                                        @{NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
                                                          NSAccessibilityAnnouncementKey: enteredText
                                                        });
        });
    }];
}

// Show a modal dialog with a single choice and OK and Cancel buttons
- (void)displayAlertWithText:(NSString *)messageText {
    _storedDialogType = kJourneyDialogSingleChoice;
    NSAlert *journeyDialog = [self journeyAlertWithText:messageText];
    if (!journeyDialog)
        return;

    [journeyDialog addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [journeyDialog addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    JourneyMenuHandler __weak *weakSelf = self;

    [journeyDialog beginSheetModalForWindow:self.delegate.window completionHandler:^(NSInteger result){
        if (result == NSAlertFirstButtonReturn) {
            [weakSelf sendCorrespondingMouseEventForMenuItem:weakSelf.journeyDialogMenuItems.firstObject];
        } else {
            [weakSelf sendCorrespondingMouseEventForMenuItem:weakSelf.journeyDialogMenuItems.lastObject];
        }
        [self dialogEnded];
    }];
}

// Show a modal dialog with messagetext and popup menu multiple choice + OK and Cancel buttons
- (void)displayPopupMenuWithMessageText:(NSString *)messageText {
    _storedDialogType = kJourneyDialogMultipleChoice;
    NSAlert *journeyDialog = [self journeyAlertWithText:messageText];
    if (!journeyDialog)
        return;

    if ([_journeyDialogClosedTimestamp timeIntervalSinceNow] > -1 && [messageText hasPrefix:@"Musings:"]) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self.delegate speakMostRecent:nil];
        });
    }

    _journeyLastDialogMenuItems = _journeyDialogMenuItems;

    NSMutableArray *strings = [NSMutableArray new];
    for (JourneyMenuItem *dialogMenuItem in _journeyDialogMenuItems) {
        if (![dialogMenuItem.title isEqualToString:@"[cancel]"])
            [strings addObject:dialogMenuItem.title];
    }
    NSView *accessoryView = [self objectMenuAccessoryViewWithStrings:strings];

    [journeyDialog addButtonWithTitle:NSLocalizedString(@"OK", nil)];
    [journeyDialog addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    journeyDialog.accessoryView = accessoryView;
    journeyDialog.window.initialFirstResponder = _journeyPopupButton;

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
        [self dialogEnded];
    }];
}

- (void)dialogEnded {
    _journeyDialogClosedTimestamp = [NSDate date];
    _reallyShowingDialog = NO;
    shouldStartNewJourneyDialogMenu = YES;
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

// We must keep a copy of the _journeyPartyMenuItems arrays in order to know when to display a new dialog
- (BOOL)partyIsEqualToLast {
    if (_journeyLastPartyMenuItems.count == 0 || _journeyLastPartyMenuItems.count != _journeyPartyMenuItems.count) {
        return NO;
    }
    for (NSUInteger i = 0; i < _journeyLastPartyMenuItems.count; i++) {
        if (![_journeyLastPartyMenuItems[i].title isEqualToString:_journeyPartyMenuItems[i].title]) {
            return NO;
        }
    }
    return YES;
}

- (void)deleteAllJourneyMenus {
    [self.journeyPartyMenu.submenu removeAllItems];
    _journeyPartyMenuItems = [NSMutableArray<JourneyMenuItem *> new];
    shouldStartNewJourneyPartyMenu = YES;
    shouldStartNewJourneyDialogMenu = YES;
    self.journeyPartyMenu.hidden = YES;
    _shouldShowDialog = NO;
    _journeyDialogMenuItems = [NSMutableArray<JourneyMenuItem *> new];
    _journeyDialogClosedTimestamp = [NSDate date];
    [self deleteMembersMenu];
}

// Needed in order to hide the Individual Commands menu at game end
// Does this only happen after the game ends?
- (void)deleteMembersMenu {
    [self.journeyMembersMenu.submenu removeAllItems];
    _journeyVerbMenuItems = [NSMutableDictionary<NSNumber *,JourneyMenuItem *> new];
    _journeyGlueStrings = [NSMutableArray<JourneyMenuItem *> new];
    _capturedMembersMenu = [NSMutableArray<NSMenuItem *> new];
    shouldStartNewJourneyMembersMenu = YES;
    self.journeyMembersMenu.hidden = YES;
}

- (void)hideJourneyMenus {
    self.journeyMembersMenu.hidden = YES;
    self.journeyPartyMenu.hidden = YES;
}

- (void)showJourneyMenus {
    if (!_delegate.voiceOverActive || _delegate.gameID != kGameIsJourney || !_delegate.isAlive)
        return;

    self.journeyMembersMenu.hidden = (self.journeyMembersMenu.submenu.itemArray.count == 0);

    if (self.journeyPartyMenu.submenu.itemArray.count == 0 && _journeyPartyMenuItems.count) {
        for (JourneyMenuItem *item in _journeyPartyMenuItems) {
            [self.journeyPartyMenu.submenu addItem:[[NSMenuItem alloc] initWithTitle:item.title action:@selector(journeyPartyAction:) keyEquivalent:@""]];
        }
    }

    self.journeyPartyMenu.hidden = (self.journeyPartyMenu.submenu.itemArray.count == 0);
}

- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *)buf length:(NSUInteger)len {

    if (_delegate.gameID != kGameIsJourney)
        return;

#pragma mark DeleteAll

    if (type == kJMenuTypeDeleteAll) {
        [self deleteAllJourneyMenus];
        return;
    } else if (type == kJMenuTypeDeleteMembers) {
        [self deleteMembersMenu];
        return;
    }

#pragma mark TextEntry

    if (type == kJMenuTypeTextEntry) {
        BOOL elvish = stopflag;
        NSString *messageText;
        if (elvish) {
            if (_journeyVerbMenuItems.count == 0)
                return;
            for (JourneyMenuItem *actor in _journeyVerbMenuItems.allValues) {
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

    unichar cstring[20];
    for (NSUInteger i = 0; i < len; i++) {
        cstring[i] = (unichar)buf[i];
    }
    NSString *str = [NSString stringWithCharacters:cstring length:len];
    if (!str) {
        NSLog(@"ERROR!");
        return;
    }

    BOOL stringIsCancel = [str isEqualToString:@"[cancel]"];

    // If string it not "[cancel]", we will update the menu rather than
    // showing a dialog
    if (stopflag && !stringIsCancel) {
        _shouldShowDialog = NO;
    }

    JourneyMenuItem *item = [[JourneyMenuItem alloc] initWithTitle:str column:column line:line tag:tagCounter++];

#pragma mark Party

    switch (type) {

        case kJMenuTypeParty:
            if (self.journeyPartyMenu == nil) {
                NSLog(@"Error! No Journey Party menu");
                return;
            }

            if (shouldStartNewJourneyPartyMenu) {
                [self.journeyPartyMenu.submenu removeAllItems];
                _journeyPartyMenuItems = [NSMutableArray new];
            }

            [_journeyPartyMenuItems addObject:item];

            shouldStartNewJourneyPartyMenu = stopflag;
            if (stopflag) {
                if (stringIsCancel) {
                    if ([self partyIsEqualToLast] || _skipNextDialog) {
                        self.journeyPartyMenu.hidden = NO;
                        shouldStartNewJourneyPartyMenu = YES;
                        _skipNextDialog = NO;
                        return;
                    }
                    _journeyDialogMenuItems = _journeyPartyMenuItems;
                    _journeyLastPartyMenuItems = _journeyPartyMenuItems;
                    _shouldShowDialog = NO;
                    [self displayPopupMenuWithMessageText:@"Please Select One:"];
                    return;
                } else {
                    _journeyLastPartyMenuItems = [NSMutableArray new];
                    for (JourneyMenuItem *menuToAdd in _journeyPartyMenuItems) {
                        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:menuToAdd.title action:@selector(journeyPartyAction:) keyEquivalent:@""];
                        menuItem.tag = menuToAdd.tag;
                        [self.journeyPartyMenu.submenu addItem:menuItem];
                    }
                    if (_delegate.voiceOverActive) {
                        self.journeyPartyMenu.hidden = NO;
                    }
                }
            }
            break;
#pragma mark Members
        case kJMenuTypeMembers:
            if (self.journeyMembersMenu == nil) {
                NSLog(@"Error! No Individual Commands menu");
                return;
            }

            if (shouldStartNewJourneyMembersMenu) {
                [self.journeyMembersMenu.submenu removeAllItems];
                _journeyVerbMenuItems = [NSMutableDictionary<NSNumber *,JourneyMenuItem *> new];
            }
            [self.journeyMembersMenu.submenu addItem:[[NSMenuItem alloc] initWithTitle:str action:@selector(journeyMemberDummyAction:) keyEquivalent:@""]];
            shouldStartNewJourneyMembersMenu = stopflag;

            if (_delegate.voiceOverActive) {
                self.journeyMembersMenu.hidden = NO;
            }
            break;
#pragma mark Verbs

        case kJMenuTypeVerbs:
            if (self.journeyMembersMenu) {
                NSMenuItem *last = self.journeyMembersMenu.submenu.itemArray.lastObject;
                last.action = @selector(journeyPartyAction:);
                if (!last.submenu)
                    last.submenu = [[NSMenu alloc] initWithTitle:@""];
                NSMenuItem *menuItemToAdd = [[NSMenuItem alloc] initWithTitle:str action:@selector(journeyMemberVerbAction:) keyEquivalent:@""];
                menuItemToAdd.tag = item.tag;
                if ([menuItemToAdd.title characterAtIndex:0] == '[')
                    menuItemToAdd.action = nil;
                [last.submenu addItem:menuItemToAdd];
                item.actor = last.title;
                if (item.actor == nil) {
                    NSLog(@"ERROR!!!");
                }
                _journeyVerbMenuItems[@(item.tag)] = item;
                if (_delegate.voiceOverActive) {
                    self.journeyMembersMenu.hidden = NO;
                }
            } else {
                NSLog(@"ERROR!");
            }
            break;

#pragma mark Glue

        case kJMenuTypeGlue:
            if (shouldStartNewJourneyDialogMenu) {
                _journeyGlueStrings = [NSMutableArray new];
                _journeyDialogMenuItems = [NSMutableArray new];
                shouldStartNewJourneyDialogMenu = NO;
            }
            [_journeyGlueStrings addObject:item];
            break;

#pragma mark Objects

        case kJMenuTypeObjects:
            [_journeyDialogMenuItems addObject:item];
            shouldStartNewJourneyDialogMenu = stopflag;
            if (stopflag) {
                if (self.journeyMembersMenu == nil)
                    return;

                if (_journeyGlueStrings.count == 0) //|| _journeyVerbMenuItems.count == 0)
                    return;

                NSString *messageText = @"";
                NSUInteger actorline = _journeyGlueStrings.firstObject.line;

                for (JourneyMenuItem *actor in _journeyVerbMenuItems.allValues) {
                    if (actor.line == actorline) {
                        messageText = actor.actor;
                        break;
                    }
                }
                if (messageText == nil)
                    return;

                if (messageText.length)
                    messageText = [messageText stringByAppendingString:@","];

                for (JourneyMenuItem *glueString in _journeyGlueStrings) {
                    if (messageText.length)
                        messageText = [messageText stringByAppendingString:@" "];
                    messageText = [messageText stringByAppendingString:glueString.title];
                }

                _shouldShowDialog = NO;

//                 This does not work, because it adds the move *before* the one we want.
//                if (_journeyVerbMenuItems.count == 0 && [_journeyDialogClosedTimestamp timeIntervalSinceNow] > -1) {
//                    [_delegate flushDisplay];
//                    GlkTextBufferWindow *bwin = (GlkTextBufferWindow *)[_delegate largestWithMoves];
//                    messageText = [NSString stringWithFormat:@"\"%@\"\n\n%@",  bwin.lastMoveString, messageText];
//                }

                if (_journeyDialogMenuItems.count <= 2) {

                    // Musings dialog hack
                    if (_journeyVerbMenuItems.count == 0) {
                        messageText = [messageText stringByAppendingString:@":"];
                    }
                    messageText = [messageText stringByAppendingString:@" "];
                    messageText = [messageText stringByAppendingString:_journeyDialogMenuItems.firstObject.title];
                    [self displayAlertWithText:messageText];
                } else {
                    messageText = [messageText stringByAppendingString:@":"];
                    [self displayPopupMenuWithMessageText:messageText];
                }
                _journeyGlueStrings = [NSMutableArray new];
            }
            break;
        default:
            NSLog(@"Error!");
            break;
    }
}

- (void)captureMembersMenu {
    _capturedMembersMenu = [[NSMutableArray alloc] initWithCapacity:self.journeyMembersMenu.submenu.itemArray.count];

    for (NSMenuItem *item in self.journeyMembersMenu.submenu.itemArray) {
        [_capturedMembersMenu addObject:item];
    }
}

- (void)findCancelItemWithColumn:(NSUInteger *)column line:(NSUInteger *)line array:(NSArray *)array{
    if (_journeyPartyMenuItems.count) {
        JourneyMenuItem *item = array.lastObject;
        *column = item.column;
        *line = item.line;
    }
}

- (void)recreateDialog {
    [self showJourneyMenus];
    if ([_journeyDialogClosedTimestamp timeIntervalSinceNow] > -1) {
        return;
    }
    if (_reallyShowingDialog || [_journeyDialogClosedTimestamp timeIntervalSinceNow] > -1 || _delegate.mustBeQuiet) {
        return;
    }
    if (_restoredShowingDialog) {
        _shouldShowDialog = YES;
    }
    _restoredShowingDialog = NO;
    if (!_delegate.voiceOverActive || !_shouldShowDialog || _delegate.gameID != kGameIsJourney)
        return;
    _shouldShowDialog = NO;
    switch (_storedDialogType) {
        case kJourneyDialogTextEntry:
            [self displayAlertWithTextEntry:_storedDialogText elvish:NO];
            break;
        case kJourneyDialogTextEntryElvish:
            [self displayAlertWithTextEntry:_storedDialogText elvish:YES];
            break;
        case kJourneyDialogSingleChoice:
            [self displayAlertWithText:_storedDialogText];
            break;
        case kJourneyDialogMultipleChoice:
            [self displayPopupMenuWithMessageText:self.storedDialogText];
            break;
    }
}

- (BOOL)updateOnBecameKey:(BOOL)recreateDialog {
    if (_delegate.gameID != kGameIsJourney) {
        [self hideJourneyMenus];
    } else {
        if (!_textGridWindow)
            _textGridWindow =
            (GlkTextGridWindow *)_delegate.gwindows[@(_gridTextWinName)];
        [self recreateJourneyMenus];
        if (_delegate.mustBeQuiet) {
            NSLog(@"updateOnBecameKey: reset _journeyDialogClosedTimestamp because _mustBeQuiet is YES");
            _journeyDialogClosedTimestamp = [NSDate date];
        }
        if (recreateDialog) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [self recreateDialog];
            });
            return YES;
        }
    }
    return NO;
}

@end

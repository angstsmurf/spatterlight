//
//  JourneyMenuHandler.m
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "AppDelegate.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+Speech.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkEvent.h"
#import "JourneyMenuHandler.h"
#import "JourneyMenuItem.h"
#import "JourneyTextFormatter.h"
#import "NSString+Categories.h"
#import "DebugLog.h"

// The longest text the text entry dialogs send, and the field limits
// for elvish and for the main character's name.
static const NSUInteger kMaxEnteredTextLength = 40;
static const NSUInteger kElvishMaxLength = 13;
static const NSUInteger kNameMaxLength = 8;
// The last move text prepended to a recreated dialog is cut to this.
static const NSUInteger kMaxLastMoveLength = 500;

static const NSTimeInterval kRecentlyClosedInterval = 1;
static const NSTimeInterval kRecreateDialogDelay = 0.5;
static const NSTimeInterval kBecameKeyDelay = 0.1;
static const NSTimeInterval kSpeakMusingsDelay = 1;
static const NSTimeInterval kSpeakEnteredTextDelay = 2;

@interface JourneyMenuHandler () {
    BOOL shouldStartNewJourneyPartyMenu;
    BOOL shouldStartNewJourneyMembersMenu;
    BOOL shouldStartNewJourneyDialogMenu;
    NSInteger tagCounter;
}

@property NSPopUpButton *journeyPopupButton;
@property NSTextField *journeyTextField;

// These must be arrays, because a dictionary won't stay sorted
// in the order items are added to it
@property NSMutableArray<JourneyMenuItem *> *journeyDialogMenuItems;
@property NSMutableArray<JourneyMenuItem *> *journeyPartyMenuItems;

// We need this in order to know when to dispaly a new dialog.
// If we didn't show dialogs, we could have compared to the actual menu instead
@property NSMutableArray<JourneyMenuItem *> *journeyLastPartyMenuItems;

@property NSMutableDictionary<NSNumber *,JourneyMenuItem *> *journeyVerbMenuItems;
@property NSMutableArray<JourneyMenuItem *> *journeyGlueStrings;

@property (nonatomic) NSMenuItem *journeyMembersMenu;
@property (nonatomic) NSMenuItem *journeyPartyMenu;

@property NSMutableArray<NSMenuItem *> *capturedMembersMenu;

@property BOOL reallyShowingDialog;
@property BOOL restoredShowingDialog;
@property BOOL lastDialogAddedMove;
@property BOOL restoredDialogAddedMove;

@property kJourneyDialogType storedDialogType;
@property NSString *storedDialogText;

@end

@implementation JourneyMenuHandler

- (instancetype)initWithDelegate:(GlkController *)delegate gridWindow:(GlkTextGridWindow *)gridwindow bufferWindow:(GlkTextBufferWindow *)bufferwindow {
    self = [super init];
    if (self) {
        shouldStartNewJourneyPartyMenu = YES;
        shouldStartNewJourneyMembersMenu = YES;
        shouldStartNewJourneyDialogMenu = YES;
        self.delegate = delegate;
        self.textGridWindow = gridwindow;
        self.textBufferWindow = bufferwindow;
        self.journeyDialogClosedTimestamp = [NSDate distantPast];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        NSSet *itemArray = [NSSet setWithObjects:[NSMutableArray class], [JourneyMenuItem class], nil];
        NSSet *itemDictionary = [NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [JourneyMenuItem class], nil];
        NSSet *menuItemArray = [NSSet setWithObjects:[NSMutableArray class], [NSMenuItem class], nil];

        self.journeyDialogMenuItems = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyDialogMenuItems"] mutableCopy];
        self.journeyVerbMenuItems = [[decoder decodeObjectOfClasses:itemDictionary forKey:@"journeyVerbMenuItems"] mutableCopy];
        self.journeyPartyMenuItems = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyPartyMenuItems"] mutableCopy];
        self.journeyLastPartyMenuItems = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyLastPartyMenuItems"] mutableCopy];
        self.journeyGlueStrings = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyGlueStrings"] mutableCopy];
        self.capturedMembersMenu = [[decoder decodeObjectOfClasses:menuItemArray forKey:@"capturedMembersMenu"] mutableCopy];
        // Older autosaves have no tag counter. Start above every restored
        // verb tag, so that new verbs don't replace restored ones.
        if ([decoder containsValueForKey:@"tagCounter"]) {
            tagCounter = [decoder decodeIntegerForKey:@"tagCounter"];
        } else {
            for (NSNumber *tag in _journeyVerbMenuItems)
                tagCounter = MAX(tagCounter, tag.integerValue + 1);
        }
        self.gridTextWinName = [decoder decodeIntegerForKey:@"gridTextWinName"];
        self.bufferTextWinName = [decoder decodeIntegerForKey:@"bufferTextWinName"];
        self.shouldShowDialog = [decoder decodeBoolForKey:@"shouldShowDialog"];
        _restoredShowingDialog = self.shouldShowDialog;
        self.lastDialogAddedMove = [decoder decodeBoolForKey:@"lastDialogAddedMove"];
        self.restoredDialogAddedMove = self.lastDialogAddedMove;
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

    [encoder encodeInteger:_textGridWindow.name forKey:@"gridTextWinName"];
    [encoder encodeInteger:_textBufferWindow.name forKey:@"bufferTextWinName"];

    [encoder encodeBool:shouldStartNewJourneyDialogMenu forKey:@"shouldStartNewJourneyDialogMenu"];
    [encoder encodeBool:_shouldShowDialog forKey:@"shouldShowDialog"];
    [encoder encodeBool:_lastDialogAddedMove forKey:@"lastDialogAddedMove"];
    [encoder encodeInteger:_storedDialogType forKey:@"storedDialogType"];
    [encoder encodeObject:_storedDialogText forKey:@"storedDialogText"];
    [encoder encodeInteger:tagCounter forKey:@"tagCounter"];
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

- (void)rebuildPartySubmenu {
    NSMenu *submenu = self.journeyPartyMenu.submenu;
    [submenu removeAllItems];
    for (JourneyMenuItem *item in _journeyPartyMenuItems) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:item.title action:@selector(journeyPartyAction:) keyEquivalent:@""];
        menuItem.tag = item.tag;
        [submenu addItem:menuItem];
    }
}

// The party member whose verbs are on the given line, or nil.
- (nullable NSString *)actorOnLine:(NSUInteger)line {
    for (JourneyMenuItem *verb in _journeyVerbMenuItems.allValues) {
        if (verb.line == line)
            return verb.actor;
    }
    return nil;
}

- (BOOL)dialogClosedRecently {
    return [_journeyDialogClosedTimestamp timeIntervalSinceNow] > -kRecentlyClosedInterval;
}

- (void)recreateJourneyMenus {
    if (![self partyIsEqualToLast] && _journeyPartyMenuItems.count) {
        shouldStartNewJourneyPartyMenu = YES;
        [self rebuildPartySubmenu];
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
    if (item == nil) {
        NSLog(@"sendCorrespondingMouseEventForMenuItem: no menu item");
        return;
    }

    // A grid with a border (the Amiga look) has a '/' in the corner.
    BOOL bordered = ([_textGridWindow characterAtPoint:NSZeroPoint] == '/');
    NSPoint point;
    if (![item getClickPoint:&point inGridOfSize:[_textGridWindow currentSizeInChars] bordered:bordered]) {
        NSLog(@"Mouse click out of bounds at %@, not sent", NSStringFromPoint(point));
        [self showJourneyMenus];
        _shouldShowDialog = NO;
        return;
    }

    [_delegate markLastSeen];
    GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:point forWindow:_textGridWindow.name];
    [self showJourneyMenus];
    [self.delegate queueEvent:gev];
    _shouldShowDialog = NO;
}

- (nullable NSAlert *)journeyAlertWithText:(NSString *)text {
    if (_shouldShowDialog || _reallyShowingDialog) {
        return nil;
    }

    _skipNextDialog = NO;

    _shouldShowDialog = YES;
    _storedDialogText = text;

    if (!self.delegate.voiceOverActive) {
        [self hideJourneyMenus];
        return nil;
    }

    NSAlert *journeyDialog = [NSAlert new];

    journeyDialog.messageText = text;
    _reallyShowingDialog = YES;
    return journeyDialog;
}

- (void)prepareAlertWithText:(NSString *)text type:(kJourneyDialogType)type {
    if (!_reallyShowingDialog) {
        _journeyDialogClosedTimestamp = [NSDate distantPast];
    }
    _storedDialogText = text;
    _storedDialogType = type;
    _shouldShowDialog = YES;
}


// Shows modal dialog with text field and OK button.
// The "elvish" flag is a shorthand for all text entry that is not player name change.
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
        JourneyMenuHandler *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        NSString *enteredText = strongSelf.journeyTextField.stringValue;
        NSUInteger length = MIN(enteredText.length, kMaxEnteredTextLength);
        for (NSUInteger i = 0; i < length; i++) {
            unsigned keyPress = [enteredText characterAtIndex:i];
            keyPress = chartokeycode(keyPress);
            [strongSelf.textGridWindow sendKeypress:keyPress];
        }
        [strongSelf.textGridWindow sendKeypress:keycode_Return];
        [strongSelf.delegate markLastSeen];
        [strongSelf dialogEnded];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kSpeakEnteredTextDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [weakSelf.delegate announce:enteredText priority:NSAccessibilityPriorityHigh];
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
        [weakSelf dialogEnded];
    }];
}

// Show a modal dialog with message text and popup menu multiple choice + OK and Cancel buttons
- (void)displayPopupMenuWithMessageText:(NSString *)messageText {
    _storedDialogType = kJourneyDialogMultipleChoice;
    NSAlert *journeyDialog = [self journeyAlertWithText:messageText];
    if (!journeyDialog)
        return;

    if (self.dialogClosedRecently && [messageText hasPrefix:@"Musings:"]) {
        JourneyMenuHandler __weak *weakSelf = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kSpeakMusingsDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [weakSelf.delegate speakMostRecent:nil];
        });
    }

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
        [weakSelf dialogEnded];
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

    JourneyTextFormatter *formatter = [[JourneyTextFormatter alloc] initWithMaxLength:(elvish ? kElvishMaxLength : kNameMaxLength) elvish:elvish];

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

    if (self.journeyPartyMenu.submenu.itemArray.count == 0 && _journeyPartyMenuItems.count)
        [self rebuildPartySubmenu];

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
            messageText = [self actorOnLine:line];
            if (messageText == nil)
                return;
            messageText = [messageText stringByAppendingString:@" says..."];
        } else {
            messageText = @"Please enter a new name for the main character:";
        }
        [self prepareAlertWithText:messageText type:elvish ? kJourneyDialogTextEntryElvish : kJourneyDialogTextEntry];
        return;
    }

    NSString *str = [NSString stringWithLatin1Bytes:buf length:len];

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
                        if (_delegate.voiceOverActive) {
                            self.journeyPartyMenu.hidden = NO;
                        }
                        shouldStartNewJourneyPartyMenu = YES;
                        _skipNextDialog = NO;
                        return;
                    }
                    _journeyDialogMenuItems = [_journeyPartyMenuItems mutableCopy];
                    _journeyLastPartyMenuItems = [_journeyPartyMenuItems mutableCopy];
                    _shouldShowDialog = NO;
                    [self prepareAlertWithText:@"Please Select One:" type:kJourneyDialogMultipleChoice];
                    return;
                } else {
                    _journeyLastPartyMenuItems = [NSMutableArray new];
                    [self rebuildPartySubmenu];
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
                if (last == nil) {
                    NSLog(@"handleMenuItemOfType: verb \"%@\" before any party member", str);
                    return;
                }
                last.action = @selector(journeyPartyAction:);
                if (!last.submenu)
                    last.submenu = [[NSMenu alloc] initWithTitle:@""];
                NSMenuItem *menuItemToAdd = [[NSMenuItem alloc] initWithTitle:str action:@selector(journeyMemberVerbAction:) keyEquivalent:@""];
                menuItemToAdd.tag = item.tag;
                if ([str hasPrefix:@"["])
                    menuItemToAdd.action = nil;
                [last.submenu addItem:menuItemToAdd];
                item.actor = last.title;
                _journeyVerbMenuItems[@(item.tag)] = item;
                if (_delegate.voiceOverActive) {
                    self.journeyMembersMenu.hidden = NO;
                }
            } else {
                NSLog(@"handleMenuItemOfType: No Individual Commands menu");
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
                if (self.journeyMembersMenu == nil || _journeyGlueStrings.count == 0)
                    return;

                NSUInteger actorline = _journeyGlueStrings.firstObject.line;

                // Hack for final game action,
                // where the verb is not lined up with the actor.
                if (actorline == 2 && [_journeyGlueStrings.firstObject.title isEqualToString:@"Mix"])
                    actorline = 4;

                // No actor (the Musings dialog) is fine, but an actor
                // without a name is not.
                NSString *messageText = @"";
                for (JourneyMenuItem *verb in _journeyVerbMenuItems.allValues) {
                    if (verb.line == actorline) {
                        messageText = verb.actor;
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

                if (_journeyDialogMenuItems.count <= 2) {

                    // Musings dialog hack
                    if (_journeyVerbMenuItems.count == 0) {
                        messageText = [messageText stringByAppendingString:@":"];
                    }
                    messageText = [messageText stringByAppendingString:@" "];
                    messageText = [messageText stringByAppendingString:_journeyDialogMenuItems.firstObject.title];
                    [self prepareAlertWithText:messageText type:kJourneyDialogSingleChoice];
                } else {
                    messageText = [messageText stringByAppendingString:@":"];
                    [self prepareAlertWithText:messageText type:kJourneyDialogMultipleChoice];
                }
                _journeyGlueStrings = [NSMutableArray new];
            }
            break;
        default:
            NSLog(@"handleMenuItemOfType: Unhandled switch case!");
            break;
    }
}

- (void)captureMembersMenu {
    _capturedMembersMenu = [self.journeyMembersMenu.submenu.itemArray mutableCopy];
}

- (void)recreateDialog {
    [self showJourneyMenus];

    if (_reallyShowingDialog || self.dialogClosedRecently || _delegate.mustBeQuiet || _delegate.shouldShowAutorestoreAlert) {
        return;
    }

    if (_restoredShowingDialog) {
        _shouldShowDialog = YES;
    }
    _restoredShowingDialog = NO;

    if (!_delegate.voiceOverActive || !_shouldShowDialog || _delegate.gameID != kGameIsJourney) {
        return;
    }

    [_textBufferWindow setLastMove];
    _lastDialogAddedMove = (_textBufferWindow.lastNewTextOnTurn == _delegate.turns);

    // Whether the current move has printed text
    // that we should add to the dialog
    // (to make VoiceOver read it to the player)
    if (_restoredDialogAddedMove) {
        _lastDialogAddedMove = YES;
    }
    _restoredDialogAddedMove = NO;

    _shouldShowDialog = NO;

    if (_textBufferWindow.lastNewTextOnTurn == _delegate.turns || _lastDialogAddedMove) {
        NSString *lastMoveString = [_textBufferWindow.lastMoveString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (lastMoveString.length == 0) {
            lastMoveString = _textBufferWindow.textview.string;
        }
        if (lastMoveString.length > kMaxLastMoveLength) {
            NSRange lastNewLine = [lastMoveString rangeOfString:@"\n" options:NSBackwardsSearch];
            if (lastNewLine.location == NSNotFound)
                lastMoveString = [lastMoveString substringFromIndex:lastMoveString.length - kMaxLastMoveLength];
            else
                lastMoveString = [lastMoveString substringFromIndex:lastNewLine.location + 1];
        }

        _storedDialogText = [NSString stringWithFormat:@"\"%@\"\n\n%@",  lastMoveString, _storedDialogText];
    }

    JourneyMenuHandler __weak *weakSelf = self;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kRecreateDialogDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        JourneyMenuHandler *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        switch (strongSelf.storedDialogType) {
            case kJourneyDialogTextEntry:
                [strongSelf displayAlertWithTextEntry:strongSelf.storedDialogText elvish:NO];
                break;
            case kJourneyDialogTextEntryElvish:
                [strongSelf displayAlertWithTextEntry:strongSelf.storedDialogText elvish:YES];
                break;
            case kJourneyDialogSingleChoice:
                [strongSelf displayAlertWithText:strongSelf.storedDialogText];
                break;
            case kJourneyDialogMultipleChoice:
                [strongSelf displayPopupMenuWithMessageText:strongSelf.storedDialogText];
                break;
        }
    });
}

- (BOOL)updateOnBecameKey:(BOOL)shouldRecreateDialog {
    if (_delegate.gameID != kGameIsJourney) {
        [self hideJourneyMenus];
    } else {
        if (!_textGridWindow)
            _textGridWindow =
            (GlkTextGridWindow *)_delegate.gwindows[@(_gridTextWinName)];
        if (!_textBufferWindow)
            _textBufferWindow =
            (GlkTextBufferWindow *)_delegate.gwindows[@(_bufferTextWinName)];
        if (![_textGridWindow isKindOfClass:[GlkTextGridWindow class]])
            _textGridWindow = nil;
        if (![_textBufferWindow isKindOfClass:[GlkTextBufferWindow class]])
            _textBufferWindow = nil;
        if (_textGridWindow == nil || _textBufferWindow == nil)
            return NO;
        [self recreateJourneyMenus];
        if (_delegate.mustBeQuiet) {
            _journeyDialogClosedTimestamp = [NSDate date];
        }
        if (shouldRecreateDialog) {
            JourneyMenuHandler __weak *weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kBecameKeyDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [weakSelf recreateDialog];
            });
            return YES;
        }
    }
    return NO;
}

- (void)flushDisplay {
    if (_shouldShowDialog && _delegate.turns > 1) {
        [self recreateDialog];
    }
}

@end

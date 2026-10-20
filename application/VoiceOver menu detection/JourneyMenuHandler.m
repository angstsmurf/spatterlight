//
//  JourneyMenuHandler.m
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "AppDelegate.h"
#import "GlkController.h"
#import "GlkController+InterpreterGlue.h"
#import "GlkController+MenuItems.h"
#import "GlkController+Speech.h"
#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkEvent.h"
#import "JourneyDialogPresenter.h"
#import "JourneyMenuHandler_Private.h"
#import "JourneyMenuItem.h"
#import "NSString+Categories.h"
#import "DebugLog.h"

// The longest text the text entry dialogs send.
static const NSUInteger kMaxEnteredTextLength = 40;
// The last move text prepended to a recreated dialog is cut to this.
static const NSUInteger kMaxLastMoveLength = 500;

static const NSTimeInterval kRecentlyClosedInterval = 1;
static const NSTimeInterval kRecreateDialogDelay = 0.5;
static const NSTimeInterval kBecameKeyDelay = 0.1;
static const NSTimeInterval kSpeakMusingsDelay = 1;
static const NSTimeInterval kSpeakEnteredTextDelay = 2;

// The last item of a menu that is a question.
static NSString * const kCancelTitle = @"[cancel]";

@interface JourneyMenuHandler ()

@property (nonatomic) NSMenuItem *membersMenu;
@property (nonatomic) NSMenuItem *partyMenu;
// Bumped by cancelPendingSpeech; a deferred announcement made under an
// older value is dropped.
@property NSUInteger speechGeneration;

@end

@implementation JourneyMenuHandler

- (instancetype)initWithGlkController:(GlkController *)glkctl gridWindow:(GlkTextGridWindow *)gridWindow bufferWindow:(GlkTextBufferWindow *)bufferWindow {
    self = [super init];
    if (self) {
        _startNewPartyMenu = YES;
        _startNewMembersMenu = YES;
        _startNewDialogMenu = YES;
        _glkctl = glkctl;
        _textGridWindow = gridWindow;
        _textBufferWindow = bufferWindow;
        _dialogClosedTimestamp = [NSDate distantPast];
    }
    return self;
}

- (void)cancelPendingSpeech {
    _speechGeneration++;
}

// Runs block after delay unless cancelPendingSpeech is called before then.
- (void)afterDelay:(NSTimeInterval)delay speak:(void (^)(GlkController *glkctl))block {
    NSUInteger generation = _speechGeneration;
    JourneyMenuHandler __weak *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        JourneyMenuHandler *strongSelf = weakSelf;
        if (strongSelf.glkctl && strongSelf.speechGeneration == generation)
            block(strongSelf.glkctl);
    });
}

- (JourneyDialogPresenter *)presenter {
    if (_presenter == nil)
        _presenter = [JourneyDialogPresenter new];
    return _presenter;
}

#pragma mark - Menus

- (NSMenuItem *)partyMenu {
    if (_partyMenu == nil) {
        _partyMenu = ((AppDelegate *)NSApplication.sharedApplication.delegate).journeyPartyMenuItem;
    }
    return _partyMenu;
}

- (NSMenuItem *)membersMenu {
    if (_membersMenu == nil) {
        _membersMenu = ((AppDelegate *)NSApplication.sharedApplication.delegate).journeyIndividualCommandsMenuItem;
    }
    return _membersMenu;
}

- (BOOL)membersMenuMatchesArray {
    NSArray<NSMenuItem *> *itemArray = self.membersMenu.submenu.itemArray;
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
    NSMenu *submenu = self.partyMenu.submenu;
    [submenu removeAllItems];
    for (JourneyMenuItem *item in _partyMenuItems) {
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:item.title action:@selector(journeyPartyAction:) keyEquivalent:@""];
        menuItem.tag = item.tag;
        [submenu addItem:menuItem];
    }
}

// The verb on the given line, or nil.
- (nullable JourneyMenuItem *)verbOnLine:(NSUInteger)line {
    for (JourneyMenuItem *verb in _verbMenuItems.allValues) {
        if (verb.line == line)
            return verb;
    }
    return nil;
}

// Whether the party menu is the one last shown as a dialog.
- (BOOL)partyIsEqualToLast {
    if (_lastPartyMenuItems.count == 0 || _lastPartyMenuItems.count != _partyMenuItems.count) {
        return NO;
    }
    for (NSUInteger i = 0; i < _lastPartyMenuItems.count; i++) {
        if (![_lastPartyMenuItems[i].title isEqualToString:_partyMenuItems[i].title]) {
            return NO;
        }
    }
    return YES;
}

- (void)recreateMenus {
    if (![self partyIsEqualToLast] && _partyMenuItems.count) {
        _startNewPartyMenu = YES;
        [self rebuildPartySubmenu];
    }

    if (![self membersMenuMatchesArray] && _capturedMembersMenu.count) {
        [self.membersMenu.submenu removeAllItems];
        for (NSMenuItem *item in _capturedMembersMenu)
            [_membersMenu.submenu addItem:item];
    }

    [self showMenus];

    _startNewMembersMenu = YES;
    _startNewPartyMenu = YES;
}

- (void)captureMembersMenu {
    _capturedMembersMenu = [self.membersMenu.submenu.itemArray mutableCopy];
}

- (void)deleteAllMenus {
    [self.partyMenu.submenu removeAllItems];
    _partyMenuItems = [NSMutableArray<JourneyMenuItem *> new];
    _startNewPartyMenu = YES;
    _startNewDialogMenu = YES;
    self.partyMenu.hidden = YES;
    [self cancelPendingDialog];
    _dialogMenuItems = [NSMutableArray<JourneyMenuItem *> new];
    _dialogClosedTimestamp = [NSDate date];
    [self deleteMembersMenu];
}

// Needed in order to hide the Individual Commands menu at game end
// Does this only happen after the game ends?
- (void)deleteMembersMenu {
    [self.membersMenu.submenu removeAllItems];
    _verbMenuItems = [NSMutableDictionary<NSNumber *,JourneyMenuItem *> new];
    _glueStrings = [NSMutableArray<JourneyMenuItem *> new];
    _capturedMembersMenu = [NSMutableArray<NSMenuItem *> new];
    _startNewMembersMenu = YES;
    self.membersMenu.hidden = YES;
}

- (void)hideMenus {
    self.membersMenu.hidden = YES;
    self.partyMenu.hidden = YES;
}

- (void)showMenus {
    if (!_glkctl.voiceOverActive || _glkctl.gameID != kGameIsJourney || !_glkctl.isAlive)
        return;

    self.membersMenu.hidden = (self.membersMenu.submenu.itemArray.count == 0);

    if (self.partyMenu.submenu.itemArray.count == 0 && _partyMenuItems.count)
        [self rebuildPartySubmenu];

    self.partyMenu.hidden = (self.partyMenu.submenu.itemArray.count == 0);
}

- (void)revealMenu:(NSMenuItem *)menu {
    if (_glkctl.voiceOverActive)
        menu.hidden = NO;
}

#pragma mark - Menu items from the interpreter

- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *)buf length:(NSUInteger)len {

    if (_glkctl.gameID != kGameIsJourney)
        return;

    switch (type) {
        case kJMenuTypeDeleteAll:
            [self deleteAllMenus];
            return;
        case kJMenuTypeDeleteMembers:
            [self deleteMembersMenu];
            return;
        case kJMenuTypeTextEntry:
            // stopflag is the elvish flag here.
            [self addTextEntryOnLine:line elvish:stopflag];
            return;
        default:
            break;
    }

    NSString *str = [NSString stringWithLatin1Bytes:buf length:len];

    // A menu that doesn't end in "[cancel]" is a plain menu, not a
    // question, so a question waiting to be shown is dropped.
    if (stopflag && ![str isEqualToString:kCancelTitle]) {
        [self cancelPendingDialog];
    }

    JourneyMenuItem *item = [[JourneyMenuItem alloc] initWithTitle:str column:column line:line tag:_tagCounter++];

    switch (type) {
        case kJMenuTypeParty:
            [self addPartyItem:item last:stopflag];
            break;
        case kJMenuTypeMembers:
            [self addMember:item last:stopflag];
            break;
        case kJMenuTypeVerbs:
            [self addVerb:item];
            break;
        case kJMenuTypeGlue:
            [self addGlueString:item];
            break;
        case kJMenuTypeObjects:
            [self addObject:item last:stopflag];
            break;
        default:
            NSLog(@"handleMenuItemOfType: Unhandled menu type %d", type);
            break;
    }
}

// The game asks for the main character's name, or for some text from a
// party member (the "elvish" text).
- (void)addTextEntryOnLine:(NSUInteger)line elvish:(BOOL)elvish {
    NSString *text;
    if (elvish) {
        NSString *actor = [self verbOnLine:line].actor;
        if (actor == nil)
            return;
        text = [actor stringByAppendingString:@" says..."];
    } else {
        text = @"Please enter a new name for the main character:";
    }
    [self prepareDialogWithText:text type:elvish ? JourneyDialogTypeTextEntryElvish : JourneyDialogTypeTextEntry];
}

// The party commands. A party menu that ends in "[cancel]" is a question,
// asked as a dialog unless it is the same one as last time.
- (void)addPartyItem:(JourneyMenuItem *)item last:(BOOL)last {
    if (self.partyMenu == nil) {
        NSLog(@"Error! No Journey Party menu");
        return;
    }

    if (_startNewPartyMenu) {
        [self.partyMenu.submenu removeAllItems];
        _partyMenuItems = [NSMutableArray new];
    }

    [_partyMenuItems addObject:item];

    _startNewPartyMenu = last;
    if (!last)
        return;

    if ([item.title isEqualToString:kCancelTitle]) {
        if ([self partyIsEqualToLast] || _skipNextDialog) {
            [self revealMenu:self.partyMenu];
            _startNewPartyMenu = YES;
            _skipNextDialog = NO;
            return;
        }
        _dialogMenuItems = [_partyMenuItems mutableCopy];
        _lastPartyMenuItems = [_partyMenuItems mutableCopy];
        [self prepareDialogWithText:@"Please Select One:" type:JourneyDialogTypeMultipleChoice];
    } else {
        _lastPartyMenuItems = [NSMutableArray new];
        [self rebuildPartySubmenu];
        [self revealMenu:self.partyMenu];
    }
}

// A party member, the title of a submenu of verbs.
- (void)addMember:(JourneyMenuItem *)item last:(BOOL)last {
    if (self.membersMenu == nil) {
        NSLog(@"Error! No Individual Commands menu");
        return;
    }

    if (_startNewMembersMenu) {
        [self.membersMenu.submenu removeAllItems];
        _verbMenuItems = [NSMutableDictionary<NSNumber *,JourneyMenuItem *> new];
    }
    // The action of an archived menu item is kept by name. Nothing
    // implements this one, so a member without verbs is disabled.
    SEL dummyAction = NSSelectorFromString(@"journeyMemberDummyAction:");
    [self.membersMenu.submenu addItem:[[NSMenuItem alloc] initWithTitle:item.title action:dummyAction keyEquivalent:@""]];
    _startNewMembersMenu = last;

    [self revealMenu:self.membersMenu];
}

// A verb of the party member added last.
- (void)addVerb:(JourneyMenuItem *)item {
    if (self.membersMenu == nil) {
        NSLog(@"handleMenuItemOfType: No Individual Commands menu");
        return;
    }

    NSMenuItem *member = self.membersMenu.submenu.itemArray.lastObject;
    if (member == nil) {
        NSLog(@"handleMenuItemOfType: verb \"%@\" before any party member", item.title);
        return;
    }
    member.action = @selector(journeyPartyAction:);
    if (!member.submenu)
        member.submenu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:item.title action:@selector(journeyMemberVerbAction:) keyEquivalent:@""];
    menuItem.tag = item.tag;
    // Bracketed verbs are labels.
    if ([item.title hasPrefix:@"["])
        menuItem.action = nil;
    [member.submenu addItem:menuItem];
    item.actor = member.title;
    _verbMenuItems[@(item.tag)] = item;

    [self revealMenu:self.membersMenu];
}

// The words of a question about objects, such as "Cast", "elevation" and
// "at". They come before the objects.
- (void)addGlueString:(JourneyMenuItem *)item {
    if (_startNewDialogMenu) {
        _glueStrings = [NSMutableArray new];
        _dialogMenuItems = [NSMutableArray new];
        _startNewDialogMenu = NO;
    }
    [_glueStrings addObject:item];
}

// The objects a question is about. The last one makes the question,
// asked by the party member whose verb is on the line of the first glue
// word. A question with one object and "[cancel]" is a yes-or-no one.
- (void)addObject:(JourneyMenuItem *)item last:(BOOL)last {
    [_dialogMenuItems addObject:item];
    _startNewDialogMenu = last;
    if (!last || self.membersMenu == nil || _glueStrings.count == 0)
        return;

    NSUInteger actorLine = _glueStrings.firstObject.line;

    // Hack for final game action,
    // where the verb is not lined up with the actor.
    if (actorLine == 2 && [_glueStrings.firstObject.title isEqualToString:@"Mix"])
        actorLine = 4;

    // No verb on the line (the Musings dialog) is fine, but a verb
    // without an actor is not.
    JourneyMenuItem *verb = [self verbOnLine:actorLine];
    NSString *actor = verb ? verb.actor : @"";
    if (actor == nil)
        return;

    NSMutableArray<NSString *> *words = [NSMutableArray new];
    if (actor.length)
        [words addObject:[actor stringByAppendingString:@","]];
    for (JourneyMenuItem *glueString in _glueStrings)
        [words addObject:glueString.title];
    NSString *text = [words componentsJoinedByString:@" "];

    if (_dialogMenuItems.count <= 2) {
        // Musings dialog hack
        if (_verbMenuItems.count == 0)
            text = [text stringByAppendingString:@":"];
        text = [text stringByAppendingFormat:@" %@", _dialogMenuItems.firstObject.title];
        [self prepareDialogWithText:text type:JourneyDialogTypeSingleChoice];
    } else {
        text = [text stringByAppendingString:@":"];
        [self prepareDialogWithText:text type:JourneyDialogTypeMultipleChoice];
    }
    _glueStrings = [NSMutableArray new];
}

#pragma mark - Choosing

- (void)partyAction:(id)sender {
    NSMenuItem *senderItem = (NSMenuItem *)sender;
    for (JourneyMenuItem *item in _partyMenuItems) {
        if ([item.title isEqualToString:senderItem.title]) {
            [self sendMouseEventForMenuItem:item];
            return;
        }
    }
}

- (void)memberVerbAction:(id)sender {
    NSMenuItem *senderItem = (NSMenuItem *)sender;
    JourneyMenuItem *item = _verbMenuItems[@(senderItem.tag)];
    if (!item)
        NSLog(@"Error! Verb menu item %@ not found!", senderItem.title);
    else
        [self sendMouseEventForMenuItem:item];
}

// Clicks the grid where the item is. This answers any question waiting
// to be shown.
- (void)sendMouseEventForMenuItem:(JourneyMenuItem *)item {
    if (_dialogState != JourneyDialogStateShowing)
        _dialogState = JourneyDialogStateIdle;

    if (item == nil) {
        NSLog(@"sendMouseEventForMenuItem: no menu item");
        return;
    }

    // A grid with a border (the Amiga look) has a '/' in the corner.
    BOOL bordered = ([_textGridWindow characterAtPoint:NSZeroPoint] == '/');
    NSPoint point;
    if (![item getClickPoint:&point inGridOfSize:[_textGridWindow currentSizeInChars] bordered:bordered]) {
        NSLog(@"Mouse click out of bounds at %@, not sent", NSStringFromPoint(point));
        [self showMenus];
        return;
    }

    [_glkctl markLastSeen];
    GlkEvent *gev = [[GlkEvent alloc] initMouseEvent:point forWindow:_textGridWindow.name];
    [self showMenus];
    [_glkctl queueEvent:gev];
}

#pragma mark - Dialog state

- (BOOL)dialogClosedRecently {
    return [_dialogClosedTimestamp timeIntervalSinceNow] > -kRecentlyClosedInterval;
}

- (BOOL)hasPendingDialog {
    return _dialogState == JourneyDialogStatePending || _dialogState == JourneyDialogStateRestored;
}

- (void)cancelPendingDialog {
    if (_dialogState == JourneyDialogStatePending)
        _dialogState = JourneyDialogStateIdle;
}

// A question to show at the next display update. A dialog on screen
// stays as it is.
- (void)prepareDialogWithText:(NSString *)text type:(JourneyDialogType)type {
    _dialogText = text;
    _dialogType = type;
    if (_dialogState == JourneyDialogStateShowing)
        return;
    _dialogClosedTimestamp = [NSDate distantPast];
    _dialogState = JourneyDialogStatePending;
}

- (void)dialogEnded {
    _dialogClosedTimestamp = [NSDate date];
    _dialogState = JourneyDialogStateIdle;
    _startNewDialogMenu = YES;
}

#pragma mark - Showing dialogs

- (void)flushDisplay {
    if (self.hasPendingDialog && _glkctl.turnsThisSession > 1) {
        [self recreateDialog];
    }
}

- (BOOL)updateOnBecameKey:(BOOL)shouldRecreateDialog {
    if (_glkctl.gameID != kGameIsJourney) {
        [self hideMenus];
        return NO;
    }

    if (!_textGridWindow)
        _textGridWindow = (GlkTextGridWindow *)_glkctl.gwindows[@(_gridTextWinName)];
    if (!_textBufferWindow)
        _textBufferWindow = (GlkTextBufferWindow *)_glkctl.gwindows[@(_bufferTextWinName)];
    if (![_textGridWindow isKindOfClass:[GlkTextGridWindow class]])
        _textGridWindow = nil;
    if (![_textBufferWindow isKindOfClass:[GlkTextBufferWindow class]])
        _textBufferWindow = nil;
    if (_textGridWindow == nil || _textBufferWindow == nil)
        return NO;

    [self recreateMenus];
    if (_glkctl.mustBeQuiet) {
        _dialogClosedTimestamp = [NSDate date];
    }
    if (!shouldRecreateDialog)
        return NO;

    JourneyMenuHandler __weak *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kBecameKeyDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [weakSelf recreateDialog];
    });
    return YES;
}

- (void)recreateDialog {
    [self showMenus];

    if (![self shouldRecreateDialog])
        return;

    // Idle until it shows, so that only a new question stops it.
    _dialogState = JourneyDialogStateIdle;
    NSString *text = [self dialogTextByPrependingLastMove];

    JourneyMenuHandler __weak *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kRecreateDialogDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [weakSelf presentStoredDialogWithText:text];
    });
}

// Whether the question waiting can be shown now. A restored one becomes
// pending once nothing else is in the way, even if VoiceOver is off.
- (BOOL)shouldRecreateDialog {
    if (_dialogState == JourneyDialogStateShowing || self.dialogClosedRecently || _glkctl.mustBeQuiet || _glkctl.shouldShowAutorestoreAlert) {
        return NO;
    }

    if (_dialogState == JourneyDialogStateRestored)
        _dialogState = JourneyDialogStatePending;

    return _glkctl.voiceOverActive && _dialogState == JourneyDialogStatePending && _glkctl.gameID == kGameIsJourney;
}

// The question, preceded by the text the last move printed, if any, so
// that VoiceOver reads that too.
- (NSString *)dialogTextByPrependingLastMove {
    [_textBufferWindow setLastMove];
    _dialogIncludesLastMove = (_textBufferWindow.lastNewTextOnTurn == _glkctl.turns) || _restoredDialogIncludesLastMove;
    _restoredDialogIncludesLastMove = NO;

    NSString *text = _dialogText ?: @"";
    if (!_dialogIncludesLastMove)
        return text;

    NSString *lastMoveString = [_textBufferWindow.lastMoveString stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (lastMoveString.length == 0) {
        lastMoveString = _textBufferWindow.textview.string ?: @"";
    }
    if (lastMoveString.length > kMaxLastMoveLength) {
        NSRange lastNewLine = [lastMoveString rangeOfString:@"\n" options:NSBackwardsSearch];
        if (lastNewLine.location == NSNotFound)
            lastMoveString = [lastMoveString substringFromIndex:lastMoveString.length - kMaxLastMoveLength];
        else
            lastMoveString = [lastMoveString substringFromIndex:lastNewLine.location + 1];
    }

    return [NSString stringWithFormat:@"\"%@\"\n\n%@", lastMoveString, text];
}

- (void)presentStoredDialogWithText:(NSString *)text {
    // A new question came while this one waited.
    if (_dialogState != JourneyDialogStateIdle)
        return;

    _skipNextDialog = NO;

    if (!_glkctl.voiceOverActive) {
        _dialogState = JourneyDialogStatePending;
        [self hideMenus];
        return;
    }

    _dialogState = JourneyDialogStateShowing;

    NSWindow *window = _glkctl.window;
    JourneyMenuHandler __weak *weakSelf = self;

    switch (_dialogType) {
        case JourneyDialogTypeTextEntry:
        case JourneyDialogTypeTextEntryElvish: {
            [self.presenter presentTextEntryWithText:text elvish:(_dialogType == JourneyDialogTypeTextEntryElvish) forWindow:window completion:^(NSString *enteredText) {
                [weakSelf sendEnteredText:enteredText];
            }];
            break;
        }

        case JourneyDialogTypeSingleChoice: {
            [self.presenter presentConfirmationWithText:text forWindow:window completion:^(BOOL confirmed) {
                JourneyMenuHandler *strongSelf = weakSelf;
                [strongSelf sendMouseEventForMenuItem:confirmed ? strongSelf.dialogMenuItems.firstObject : strongSelf.dialogMenuItems.lastObject];
                [strongSelf dialogEnded];
            }];
            break;
        }

        case JourneyDialogTypeMultipleChoice: {
            if (self.dialogClosedRecently && [text hasPrefix:@"Musings:"]) {
                [self afterDelay:kSpeakMusingsDelay speak:^(GlkController *glkctl) {
                    [glkctl speakMostRecent:nil];
                }];
            }

            NSMutableArray<NSString *> *titles = [NSMutableArray new];
            for (JourneyMenuItem *dialogMenuItem in _dialogMenuItems) {
                if (![dialogMenuItem.title isEqualToString:kCancelTitle])
                    [titles addObject:dialogMenuItem.title];
            }

            [self.presenter presentChoiceWithText:text titles:titles forWindow:window completion:^(NSString *chosenTitle) {
                [weakSelf sendChoice:chosenTitle];
            }];
            break;
        }
    }
}

// Types the text into the grid window, ending with Return.
- (void)sendEnteredText:(NSString *)enteredText {
    NSUInteger length = MIN(enteredText.length, kMaxEnteredTextLength);
    for (NSUInteger i = 0; i < length; i++) {
        unsigned keyPress = chartokeycode([enteredText characterAtIndex:i]);
        [_textGridWindow sendKeypress:keyPress];
    }
    [_textGridWindow sendKeypress:keycode_Return];
    [_glkctl markLastSeen];
    [self dialogEnded];

    [self afterDelay:kSpeakEnteredTextDelay speak:^(GlkController *glkctl) {
        [glkctl announce:enteredText priority:NSAccessibilityPriorityHigh];
    }];
}

// Clicks the dialog item with the chosen title, or the last one
// ("[cancel]") if there is none.
- (void)sendChoice:(nullable NSString *)chosenTitle {
    if (chosenTitle == nil) {
        [self sendMouseEventForMenuItem:_dialogMenuItems.lastObject];
    } else {
        for (JourneyMenuItem *dialogMenuItem in _dialogMenuItems) {
            if ([dialogMenuItem.title isEqualToString:chosenTitle]) {
                [self sendMouseEventForMenuItem:dialogMenuItem];
                break;
            }
        }
    }
    [self dialogEnded];
}

@end

#import "PreferencesPrivate.h"

#import "Constants.h"
#import "Game.h"
#import "GlkController.h"
#import "GlkStyle.h"
#import "Metadata.h"
#import "NSString+Categories.h"
#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "TableViewController+LibraryManagement.h"
#import "TableViewController+GameActions.h"
#import "Theme.h"
#import "ThemeArrayController.h"
#import "Fetches.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"

@implementation Preferences (Themes)

#pragma mark Color mode changes

// The system's distributed notification. It can arrive before the new mode
// is readable, so the check is repeated shortly afterwards; a switch is only
// announced once, whichever check sees it (see noteAppearanceMayHaveChanged).
- (void)noteColorModeChanged:(NSNotification *)notification {
    [Preferences noteAppearanceMayHaveChanged];
    [Preferences scheduleAppearanceRechecks];
}

static char SPEffectiveAppearanceContext;

// AppKit's own change of NSApp.effectiveAppearance, which it makes only once
// the new system mode is in effect. It does not fire while an override pins
// the app's appearance; the distributed notification covers that case.
- (void)startObservingEffectiveAppearance {
    if (@available(macOS 10.14, *))
        [NSApp addObserver:self forKeyPath:@"effectiveAppearance" options:0 context:&SPEffectiveAppearanceContext];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notify {
    NSSet *updatedObjects = (notify.userInfo)[NSUpdatedObjectsKey];
    NSSet *insertedObjects = (notify.userInfo)[NSInsertedObjectsKey];
    NSSet *refreshedObjects = (notify.userInfo)[NSRefreshedObjectsKey];
    NSSet *deletedObjects = (notify.userInfo)[NSDeletedObjectsKey];

    if (!updatedObjects)
        updatedObjects = [NSSet new];

    updatedObjects = [updatedObjects setByAddingObjectsFromSet:insertedObjects];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:deletedObjects];

    if ([updatedObjects containsObject:self.theme]) {
        Preferences * __weak weakSelf = self;

        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf updatePrefsPanel];
            [[NSNotificationCenter defaultCenter]
             postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:self.theme]];
        });
    }
}

#pragma mark Themes Table View Magic

static void *kArrangedThemesContext = &kArrangedThemesContext;

- (void)waitForThemesTableToSelect:(Theme *)theme {
    themeAwaitingTable = theme;
    if (!observingArrangedThemes) {
        [_arrayController addObserver:self forKeyPath:@"arrangedObjects" options:0 context:kArrangedThemesContext];
        observingArrangedThemes = YES;
    }
}

- (void)stopWaitingForThemesTable {
    themeAwaitingTable = nil;
    if (observingArrangedThemes) {
        [_arrayController removeObserver:self forKeyPath:@"arrangedObjects" context:kArrangedThemesContext];
        observingArrangedThemes = NO;
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    if (context == &SPEffectiveAppearanceContext) {
        [Preferences noteAppearanceMayHaveChanged];
        return;
    }
    if (context != kArrangedThemesContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }
    if (![_arrayController.arrangedObjects count])
        return;
    Theme *theme = themeAwaitingTable;
    [self stopWaitingForThemesTable];
    if (!theme)
        return;
    // Not from inside the array controller's change notification: select
    // once the fetch and the table reload are done. A clone made in the
    // meantime wins, or selecting the original would make the next change
    // clone it again.
    dispatch_async(dispatch_get_main_queue(), ^{
        Theme *pending = self->pendingSelectionTheme;
        [self restoreThemeSelection:pending ?: theme];
    });
}

- (void)restoreThemeSelection:(Theme *)sender {
    ThemeArrayController *arrayController = _arrayController;
    // A theme cloned outside of event handling, such as in an alert
    // completion handler, is not in the table until the context has processed
    // its changes. The selection changes this causes are not user choices.
    ignoreTableSelectionChanges = YES;
    [_managedObjectContext processPendingChanges];
    ignoreTableSelectionChanges = NO;
    if (sender == pendingSelectionTheme)
        pendingSelectionTheme = nil;
    if (arrayController.selectedTheme == sender) {
        return;
    }
    NSArray *themes = arrayController.arrangedObjects;
    if (!themes.count && sender) {
        // The array controller fetches its content in a later run loop pass
        // than the nib load, so the table can still be empty here. Select the
        // theme when the content arrives instead of giving up: the old code
        // cleared self.theme here, and the next game launch then aborted the
        // app in changeCurrentGlkController:.
        [self waitForThemesTableToSelect:sender];
        return;
    }
    [self stopWaitingForThemesTable];
    if (sender && ![themes containsObject:sender]) {
        // The theme filter lets the current theme through even if it is
        // hidden in this mode, such as the theme of the current game.
        self.theme = sender;
        ignoreTableSelectionChanges = YES;
        [arrayController rearrangeObjects];
        ignoreTableSelectionChanges = NO;
        themes = arrayController.arrangedObjects;
    }
    if (![themes containsObject:sender]) {
        // Fall back to the last theme in the table, but never leave self.theme
        // nil: everything downstream (changeCurrentGlkController: in particular)
        // assumes that a current theme exists.
        Theme *fallback = themes.lastObject ? themes.lastObject : sender;
        if (fallback)
            self.theme = fallback;
        return;
    }
    self.theme = sender;
    NSUInteger row = [themes indexOfObject:self.theme];

    disregardTableSelection = NO;

    arrayController.selectionIndex = row;
    themesTableView.allowsEmptySelection = NO;
    [themesTableView scrollRowToVisible:(NSInteger)row];
}

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == themesTableView) {
        if (ignoreTableSelectionChanges)
            return;
        // Adding a clone to the table changes the selection, sometimes more
        // than once, while the original theme is still the selected row. Until
        // restoreThemeSelection: selects the clone, these are not user choices.
        // The restore timer does not fire while the color panel tracks a drag,
        // so theme must stay the clone or every drag step would clone again.
        if (pendingSelectionTheme && _arrayController.selectedTheme != pendingSelectionTheme)
            return;
        if (disregardTableSelection == YES) {
            disregardTableSelection = NO;
            return;
        }

        Theme *selected = _arrayController.selectedTheme;
        if (!selected)
            return;
        self.theme = selected;
        // Hide the previous theme if it was only shown because it was current.
        ignoreTableSelectionChanges = YES;
        [_arrayController rearrangeObjects];
        _arrayController.selectedObjects = @[selected];
        ignoreTableSelectionChanges = NO;
        [self updatePrefsPanel];
        [self changeThemeName:self.theme.name];
        _btnRemove.enabled = self.theme.editable;

        if (_oneThemeForAll) {
            NSArray *games = [Fetches fetchAllGamesAsFaultsInContext:self.managedObjectContext];
            [self.theme addGames:[NSSet setWithArray:games]];
        } else if (_currentGame) {
            _currentGame.theme = self.theme;
        }

        // Send notification that theme has changed -- trigger configure events
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:self.theme]];
    }
}

- (void)changeThemeName:(NSString *)name {
    [[NSUserDefaults standardUserDefaults] setObject:name forKey:kThemeNameKey];
    NSString *themeString = [NSString stringWithFormat:NSLocalizedString(@"Settings for theme %@", @"Settings for theme %@"), name];
    _detailsHeader.stringValue = themeString;
    _miscHeader.stringValue = themeString;
    stylesHeaderBase = themeString;
    [self updateStylesHeader];
    _zcodeHeader.stringValue = themeString;
    _vOHeader.stringValue = themeString;
}

// In Fabulich mode, the Glk Styles tab header tells which mode is being
// edited, as changes only apply to that one.
- (void)updateStylesHeader {
    NSString *header = stylesHeaderBase ? stylesHeaderBase : [NSString stringWithFormat:NSLocalizedString(@"Settings for theme %@", @"Settings for theme %@"), self.theme.name];
    if ([Preferences fabulichMode])
        header = [header stringByAppendingString:[Preferences themeSidesAreDark] ?
                  NSLocalizedString(@" (dark mode)", @" (dark mode)") : NSLocalizedString(@" (light mode)", @" (light mode)")];
    _stylesHeader.stringValue = header;
}

- (BOOL)notDuplicate:(NSString *)string {
    return [self notDuplicate:string exceptTheme:_arrayController.selectedTheme];
}

- (BOOL)notDuplicate:(NSString *)string exceptTheme:(nullable Theme *)exception {
    for (Theme *aTheme in _arrayController.arrangedObjects) {
        if ([aTheme.name isEqualToString:string] && aTheme != exception)
            return NO;
    }
    return YES;
}

- (BOOL)control:(NSControl *)control
textShouldEndEditing:(NSText *)fieldEditor {
    Theme *renamed = [self themeForTableCellSubview:control];
    if (!renamed)
        renamed = _arrayController.selectedTheme;
    if ([self notDuplicate:fieldEditor.string exceptTheme:renamed] == NO) {
        [self showDuplicateThemeNameAlert:fieldEditor theme:renamed];
        return NO;
    }
    return YES;
}

- (void)showDuplicateThemeNameAlert:(NSText *)fieldEditor theme:(Theme *)renamed {
    NSAlert *anAlert = [NSAlert alertWithMessage:[NSString stringWithFormat:NSLocalizedString(@"The theme name \"%@\" is already in use.", @"The theme name \"%@\" is already in use."), fieldEditor.string]
                                            info:NSLocalizedString(@"Please enter another name.", @"Please enter another name.")
                                         buttons:@[NSLocalizedString(@"Okay", @"Okay"), NSLocalizedString(@"Discard Change", @"Discard Change")]];

    [anAlert beginSheetModalForWindow:self.window completionHandler:^(NSInteger result){
        if (result == NSAlertSecondButtonReturn) {
            fieldEditor.string = renamed.name;
        }
    }];
}

- (void)controlTextDidEndEditing:(NSNotification *)notification {
    if ([notification.object isKindOfClass:[NSTextField class]]) {
        NSTextField *textfield = notification.object;
        // A theme renamed from the contextual menu may not be the current one.
        Theme *renamed = [self themeForTableCellSubview:textfield];
        if (!renamed || renamed == self.theme)
            [self changeThemeName:textfield.stringValue];
    }
}

/// The theme of the themes table row that view is in, or nil.
- (nullable Theme *)themeForTableCellSubview:(NSView *)view {
    NSInteger row = [themesTableView rowForView:view];
    NSArray *themes = _arrayController.arrangedObjects;
    if (row < 0 || row >= (NSInteger)themes.count)
        return nil;
    return themes[(NSUInteger)row];
}

/// The theme that a tool menu command applies to: the clicked row if the
/// command was chosen from the contextual menu of the themes table,
/// otherwise the selected theme.
- (Theme *)themeForMenuCommand:(id)sender {
    if ([sender isKindOfClass:[NSMenuItem class]] && ((NSMenuItem *)sender).menu == themesTableView.menu) {
        NSInteger row = themesTableView.clickedRow;
        NSArray *themes = _arrayController.arrangedObjects;
        if (row >= 0 && row < (NSInteger)themes.count)
            return themes[(NSUInteger)row];
    }
    return self.theme;
}

- (IBAction)clickedOneThemeForAll:(id)sender {
    if ([sender state] == NSControlStateValueOn) {
        if (![[NSUserDefaults standardUserDefaults] boolForKey:kUseForAllAlertSuppressionKey]) {
            NSFetchRequest *fetchRequest = [Game fetchRequest];
            NSError *error = nil;
            fetchRequest.includesPropertyValues = NO;
            NSUInteger numberOfGames = [self.managedObjectContext countForFetchRequest:fetchRequest error:&error];
            Theme *mostPopularTheme = nil;
            NSUInteger highestCount = 0;
            NSUInteger currentCount = 0;
            for (Theme *t in _arrayController.arrangedObjects) {
                currentCount = t.games.count;
                if (currentCount > highestCount) {
                    highestCount = t.games.count;
                    mostPopularTheme = t;
                }
            }
            if (highestCount < numberOfGames) {
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"theme != %@", mostPopularTheme];
                NSArray *fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
                [self showUseForAllAlert:fetchedObjects];
                return;
            }
        }
    }
    self.oneThemeForAll = ([sender state] == NSControlStateValueOn);
}

- (void)showUseForAllAlert:(NSArray *)games {
    NSString *format = (games.count == 1) ? NSLocalizedString(@"%@ has individual theme settings.", @"%@ has individual theme settings.") :
    NSLocalizedString(@"%@ have individual theme settings.", @"%@ have individual theme settings.");
    NSString *message = [NSString stringWithFormat:format, [NSString stringWithSummaryOfGames:games]];
    NSString *info = [NSString stringWithFormat:NSLocalizedString(@"Would you like to use theme %@ for all games?", @"Would you like to use theme %@ for all games?"), self.theme.name];

    Preferences * __weak weakSelf = self;
    [self showOkayCancelAlertWithMessage:message
                                    info:info
                          suppressionKey:kUseForAllAlertSuppressionKey
                                 handler:^(BOOL okay) {
        if (okay) {
            weakSelf.oneThemeForAll = YES;
        } else {
            weakSelf.btnOneThemeForAll.state = NSControlStateValueOff;
        }
    }];
}

- (NSString *)themeScopeTitle {
    if (_oneThemeForAll) return NSLocalizedString(@"Theme setting for all games", @"Theme setting for all games");
    if (_currentGame == nil)
        return NSLocalizedString(@"No game is currently running", @"No game is currently running");

    // Show the file name of a game without a title, but leave its metadata alone
    NSString *title = _currentGame.metadata.title;
    if (!title.length)
        title = _currentGame.path.lastPathComponent;
    if (!title.length)
        return NSLocalizedString(@"Error: Current game has no metadata!", @"Error: Current game has no metadata!");
    return [NSString stringWithFormat:NSLocalizedString(@"Theme setting for game %@", @"Theme setting for game %@"), title];
}

- (IBAction)changeAdjustSize:(id)sender {
    _adjustSize = (BOOL)[sender state];
    [[NSUserDefaults standardUserDefaults] setBool:_adjustSize forKey:kAdjustSizeKey];
}

- (IBAction)addTheme:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    NSUInteger count = [arrayController.arrangedObjects count];
    NSUInteger row = arrayController.selectionIndex;
    if (row == NSNotFound && count > 0)
        row = count - 1;
    if (row >= count)
        row = 0;
    NSTableCellView *cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:(NSInteger)row makeIfNecessary:YES];
    if ([self notDuplicate:cellView.textField.stringValue]) {
        // For some reason, tableViewSelectionDidChange will be called twice here,
        // so we disregard the first call
        disregardTableSelection = YES;
        [arrayController add:sender];
        [self performSelector:@selector(editNewEntry:) withObject:nil afterDelay:0.1];
    } else NSBeep();
}

- (IBAction)removeTheme:(id)sender {
    [self removeThemeObject:_arrayController.selectedTheme];
}

/// The Delete item of the tool menu: like the minus button, but in the
/// contextual menu of the themes table it deletes the clicked theme.
- (IBAction)deleteTheme:(id)sender {
    [self removeThemeObject:[self themeForMenuCommand:sender]];
}

- (void)removeThemeObject:(Theme *)themeToRemove {
    ThemeArrayController *arrayController = _arrayController;
    if (!themeToRemove.editable) {
        NSBeep();
        return;
    }
    Theme *ancestor = [self visibleAncestorOf:themeToRemove];
    NSSet *orphanedGames = themeToRemove.games;
    NSSet *orphanedThemes = themeToRemove.defaultChild;
    if (pendingSelectionTheme == themeToRemove)
        pendingSelectionTheme = nil;

    if (themeToRemove != arrayController.selectedTheme) {
        // A clicked theme that is not the selected one: keep the selection.
        Theme *selected = arrayController.selectedTheme;
        ignoreTableSelectionChanges = YES;
        [arrayController removeObject:themeToRemove];
        if (selected)
            arrayController.selectedObjects = @[selected];
        ignoreTableSelectionChanges = NO;
        if (!ancestor)
            ancestor = selected;
    } else {
        NSUInteger row = arrayController.selectionIndex;
        row = (row == NSNotFound || row == 0) ? 0 : row - 1;
        [arrayController removeObject:themeToRemove];
        // Select the theme this one was cloned or created from, if it still
        // exists. Otherwise the theme above the removed one.
        if (ancestor && [arrayController.arrangedObjects containsObject:ancestor])
            arrayController.selectedObjects = @[ancestor];
        else
            arrayController.selectionIndex = row;
        if (!ancestor)
            ancestor = arrayController.selectedTheme;
    }

    [ancestor addGames:orphanedGames];
    [ancestor addDefaultChild:orphanedThemes];
}

- (IBAction)applyToSelected:(id)sender {
    [[self themeForMenuCommand:sender] addGames:[NSSet setWithArray:_libcontroller.selectedGames]];
}

- (IBAction)selectUsingTheme:(id)sender {
    [_libcontroller selectGames:[self themeForMenuCommand:sender].games];
}

- (IBAction)deleteUserThemes:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    NSArray<Theme *> *fetchedObjects = [Fetches fetchUserThemesInContext:self.managedObjectContext];
    if (fetchedObjects.count == 0)
        return;

    // The current theme may be one of the deleted ones. Pick a built-in theme
    // to take its place before deleting: a deleted theme loses its context,
    // and updatePrefsPanel would crash trying to give it new styles.
    Theme *survivor = self.theme;
    if (survivor.editable) {
        survivor = survivor.ancestor;
        if (!survivor || survivor.editable || ![self themeIsVisible:survivor])
            survivor = [self fallbackTheme];
        if (!survivor)
            survivor = self.defaultTheme;
    }

    NSMutableSet *orphanedGames = [[NSMutableSet alloc] init];

    for (Theme *t in fetchedObjects) {
        if (t.games.count || t.defaultChild.count) {
            Theme *ancestor = t.ancestor;
            if (ancestor && ![self themeIsVisible:ancestor])
                ancestor = [self fallbackTheme];
            if (ancestor && !ancestor.editable) {
                [ancestor addGames:t.games];
                [ancestor addDefaultChild:t.defaultChild];
            }
            [orphanedGames unionSet:t.games];
        }
    }

    if (pendingSelectionTheme.editable)
        pendingSelectionTheme = nil;

    // Removing the selected theme moves the selection to some other row. That
    // is not a user choice, so it must not be applied to the current game.
    self.theme = survivor;
    ignoreTableSelectionChanges = YES;
    [arrayController removeObjects:fetchedObjects];
    ignoreTableSelectionChanges = NO;

    [survivor addGames:orphanedGames];
    arrayController.selectedObjects = @[survivor];
}

/// The theme to select when t is deleted: the nearest theme it was cloned or
/// created from that is shown in the themes table, or "Default" if
/// they are all hidden.
- (nullable Theme *)visibleAncestorOf:(Theme *)t {
    Theme *ancestor = t.defaultParent;
    if (!ancestor)
        ancestor = t.ancestor;
    NSMutableSet *seen = [NSMutableSet set];
    while (ancestor && ![self themeIsVisible:ancestor] && ![seen containsObject:ancestor]) {
        [seen addObject:ancestor];
        ancestor = ancestor.defaultParent;
    }
    if (ancestor && ![self themeIsVisible:ancestor])
        ancestor = [self fallbackTheme];
    return ancestor;
}

#pragma mark Fabulich mode themes

/// The single-sided dark themes that Fabulich mode hides, and the themes
/// with a light side that take their place there. MS-DOS and DOSBox are
/// not here: they are shown in both modes, and look the same in both.
+ (NSDictionary<NSString *, NSString *> *)fabulichCounterparts {
    return @{ @"Lectrote Dark" : @"Lectrote" };
}

+ (BOOL)themeIsVisibleInCurrentMode:(Theme *)t {
    if (t.editable)
        return YES;
    if ([Preferences fabulichMode])
        return [Preferences fabulichCounterparts][t.name] == nil;
    return YES;
}

/// The current theme is always shown.
- (BOOL)themeIsVisible:(Theme *)t {
    return t == self.theme || [Preferences themeIsVisibleInCurrentMode:t];
}

- (nullable Theme *)fallbackTheme {
    return [_arrayController findThemeByName:@"Default"];
}

/// The theme that takes the place of a built-in theme hidden in the current mode.
- (nullable Theme *)counterpartOfTheme:(Theme *)t {
    if (t.editable)
        return nil;
    // Nothing is hidden outside Fabulich mode
    NSString *name = nil;
    if ([Preferences fabulichMode])
        name = [Preferences fabulichCounterparts][t.name];
    return name ? [_arrayController findThemeByName:name] : nil;
}

- (void)applyThemeFilter {
    __weak Preferences *weakSelf = self;
    _arrayController.filterPredicate = [NSPredicate predicateWithBlock:^BOOL(Theme *t, NSDictionary *bindings) {
        return [weakSelf themeIsVisible:t];
    }];
}

#pragma mark Fabulich mode

/// The "Light and dark mode" checkbox on the Global panel.
- (IBAction)toggleFabulichMode:(id)sender {
    [self setFabulichModeEnabled:([sender state] == NSControlStateValueOn)];
}

- (void)setFabulichModeEnabled:(BOOL)on {
    if (on == [Preferences fabulichMode])
        return;
    [Preferences setFabulichMode:on];
    // Each visit to Fabulich mode starts out following the system.
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:SpatterlightAppearanceOverrideKey];

    self.btnFabulichMode.state = on ? NSControlStateValueOn : NSControlStateValueOff;

    // Show the themes of the new mode. The current theme is still shown, so
    // switch to its counterpart if the new mode hides it.
    ThemeArrayController *arrayController = _arrayController;
    Theme *current = self.theme;
    ignoreTableSelectionChanges = YES;
    [arrayController rearrangeObjects];
    if (current)
        arrayController.selectedObjects = @[current];
    ignoreTableSelectionChanges = NO;
    Theme *counterpart = [self counterpartOfTheme:current];
    if (counterpart && ![Preferences themeIsVisibleInCurrentMode:current] &&
        [arrayController.arrangedObjects containsObject:counterpart]) {
        arrayController.selectedObjects = @[counterpart];
    }
    NSUInteger row = arrayController.selectionIndex;
    if (row != NSNotFound)
        [themesTableView scrollRowToVisible:(NSInteger)row];
    [_libcontroller rebuildThemesSubmenu];

    // The app goes back to following the system, and the themes switch to
    // the side of the new mode.
    [Preferences applyAppearanceOverrideToApp];
    [Preferences noteAppearanceMayHaveChanged];
    [self updateStylesHeader];
}

- (IBAction)editNewEntry:(id)sender {
    ThemeArrayController *arrayController = _arrayController;
    NSUInteger row = [arrayController.arrangedObjects indexOfObject:[self themeForMenuCommand:sender]];
    if (sender == nil || row == NSNotFound)
        row = arrayController.selectionIndex;
    if (row == NSNotFound)
        row = [arrayController.arrangedObjects count] - 1;
    if (row >= [arrayController.arrangedObjects count])
        row = 0;

    NSTableCellView* cellView = (NSTableCellView*)[themesTableView viewAtColumn:0 row:(NSInteger)row makeIfNecessary:YES];
    if ((cellView.textField).acceptsFirstResponder) {
        [cellView.window makeFirstResponder:cellView.textField];
        [themesTableView scrollRowToVisible:(NSInteger)row];
    }
}

@end

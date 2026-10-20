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
#import "Theme.h"
#import "ThemeArrayController.h"
#import "Fetches.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"

@implementation Preferences (Themes)

#pragma mark Color mode changes

+ (kModeType)currentSystemMode {
    if ([[[NSUserDefaults standardUserDefaults] valueForKey:@"AppleInterfaceStyle"] isEqualToString:@"Dark"])
        return kDarkMode;
    return kLightMode;
}

- (void)noteColorModeChanged:(NSNotification *)notification {
    if ([Preferences currentSystemMode] == kDarkMode) {
        if (_darkTheme) {
            _darkOverrideActive = YES;
            [[NSNotificationCenter defaultCenter]
             postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:_darkTheme]];
        } else {
            if (_lightOverrideActive || _darkOverrideActive)
                [self lightOrDarkOverrideWasRemoved];
            return;
        }
        _lightOverrideActive = NO;
    } else {
        if (_lightTheme) {
            _lightOverrideActive = YES;
            [[NSNotificationCenter defaultCenter]
             postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:_lightTheme]];
        } else {
            if (_lightOverrideActive || _darkOverrideActive)
                [self lightOrDarkOverrideWasRemoved];
            return;
        }
        _darkOverrideActive = NO;
    }
    [[NSNotificationCenter defaultCenter] postNotification:[NSNotification notificationWithName:kColorModeChangedNotification object:nil]];
    _themesHeader.stringValue = [self themeScopeTitle];
}

- (void)lightOrDarkOverrideWasRemoved {
    _lightOverrideActive = NO;
    _darkOverrideActive = NO;
    _themesHeader.stringValue = [self themeScopeTitle];
    if (self.window.keyWindow) {
        if (_oneThemeForAll) {
            self.oneThemeForAll = YES;
        } else if (_currentGame) {
            _currentGame.theme = self.theme;
        }
    } else if (_currentGame) {
        [Preferences.instance restoreThemeSelection:_currentGame.theme];
    }
    [[NSNotificationCenter defaultCenter] postNotification:[NSNotification notificationWithName:kColorModeChangedNotification object:nil]];
}

// The first theme with the boolean attribute flag set, which is then marked
// hardLightOrDark. Clears flag on any other theme, as only one may have it.
- (nullable Theme *)onlyThemeWithFlag:(NSString *)flag {
    NSString *predicate = [flag stringByAppendingString:@" == YES"];
    NSArray<Theme *> *fetchedObjects = [Fetches fetchObjects:@"Theme" predicate:predicate inContext:_managedObjectContext];
    Theme *theme = fetchedObjects.firstObject;
    for (Theme *wrong in fetchedObjects) {
        if (wrong != theme)
            [wrong setValue:@NO forKey:flag];
    }
    theme.hardLightOrDark = YES;
    return theme;
}

- (void)findDarkAndLightThemes {
    _darkTheme = nil;
    _lightTheme = nil;
    _lightOverrideActive = NO;
    _darkOverrideActive = NO;
    _darkTheme = [self onlyThemeWithFlag:@"hardDark"];
    _lightTheme = [self onlyThemeWithFlag:@"hardLight"];

    NSArray<Theme *> *fetchedObjects = [Fetches fetchObjects:@"Theme" predicate:@"hardLightOrDark == YES" inContext:_managedObjectContext];
    for (Theme *wrong in fetchedObjects) {
        if (!wrong.hardDark && !wrong.hardLight)
            wrong.hardLightOrDark = NO;
    }

    if (_lightTheme && _lightTheme == _darkTheme) {
        _darkTheme = nil;
        _lightTheme.hardDark = NO;
    }
}

// The Use in light/dark mode menu items toggle the matching checkbox
- (void)toggleUseInMode:(kModeType)mode {
    BOOL dark = (mode == kDarkMode);
    NSMenuItem *menuItem = dark ? _darkModeMenuItem : _lightModeMenuItem;
    NSButton *checkbox = dark ? _hardDarkCheckbox : _hardLightCheckbox;
    checkbox.state = (menuItem.state == NSControlStateValueOn) ? NSControlStateValueOff : NSControlStateValueOn;
    [self changeHardThemeForMode:mode];
    if (!(dark ? self.theme.hardDark : self.theme.hardLight)) {
        // We switched off the override. Current game (or all) gets this theme applied
        if (_oneThemeForAll) {
            self.oneThemeForAll = YES;
        } else if (_currentGame) {
            [self.theme addGames:[NSSet setWithObject:_currentGame]];
        }
    }
}

- (IBAction)useInLightMode:(id)sender {
    [self toggleUseInMode:kLightMode];
}

- (IBAction)useInDarkMode:(id)sender {
    [self toggleUseInMode:kDarkMode];
}

- (IBAction)clearLightDarkOverrides:(id)sender {
    [self findDarkAndLightThemes];
    if (_darkTheme) {
        _darkTheme.hardDark = NO;
        _darkTheme.hardLightOrDark = NO;
        _darkTheme = nil;
    }
    if (_lightTheme) {
        _lightTheme.hardLight = NO;
        _lightTheme.hardLightOrDark = NO;
        _lightTheme = nil;
    }
    [self lightOrDarkOverrideWasRemoved];
    self.theme = _arrayController.selectedTheme;
    if (_oneThemeForAll)
        self.oneThemeForAll = YES;
    else
        _currentGame.theme = self.theme;
    [self updatePrefsPanel];
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
        [self updatePrefsPanel];
        [self changeThemeName:self.theme.name];
        _btnRemove.enabled = self.theme.editable;

        if (!_lightOverrideActive && !_darkOverrideActive) {
            if (_oneThemeForAll) {
                NSArray *games = [Fetches fetchAllGamesAsFaultsInContext:self.managedObjectContext];
                [self.theme addGames:[NSSet setWithArray:games]];
            } else if (_currentGame) {
                _currentGame.theme = self.theme;
            }
        }

        // Send notification that theme has changed -- trigger configure events
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:self.theme]];
    }
}

- (void)changeThemeName:(NSString *)name {
    [[NSUserDefaults standardUserDefaults] setObject:name forKey:kThemeNameKey];
    NSString *themeString = [NSString stringWithFormat:NSLocalizedString(@"Settings for theme %@", nil), name];
    _detailsHeader.stringValue = themeString;
    _miscHeader.stringValue = themeString;
    _stylesHeader.stringValue = themeString;
    _zcodeHeader.stringValue = themeString;
    _vOHeader.stringValue = themeString;
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
    NSAlert *anAlert = [NSAlert alertWithMessage:[NSString stringWithFormat:NSLocalizedString(@"The theme name \"%@\" is already in use.", nil), fieldEditor.string]
                                            info:NSLocalizedString(@"Please enter another name.", nil)
                                         buttons:@[NSLocalizedString(@"Okay", nil), NSLocalizedString(@"Discard Change", nil)]];

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
    NSString *format = (games.count == 1) ? NSLocalizedString(@"%@ has individual theme settings.", nil) :
    NSLocalizedString(@"%@ have individual theme settings.", nil);
    NSString *message = [NSString stringWithFormat:format, [NSString stringWithSummaryOfGames:games]];
    NSString *info = [NSString stringWithFormat:NSLocalizedString(@"Would you like to use theme %@ for all games?", nil), self.theme.name];

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
    if (_lightOverrideActive) return NSLocalizedString(@"Light theme override active", nil);
    if (_darkOverrideActive) return NSLocalizedString(@"Dark theme override active", nil);

    if (_oneThemeForAll) return NSLocalizedString(@"Theme setting for all games", nil);
    if (_currentGame == nil)
        return NSLocalizedString(@"No game is currently running", nil);

    // Show the file name of a game without a title, but leave its metadata alone
    NSString *title = _currentGame.metadata.title;
    if (!title.length)
        title = _currentGame.path.lastPathComponent;
    if (!title.length)
        return NSLocalizedString(@"Error: Current game has no metadata!", nil);
    return [NSString stringWithFormat:NSLocalizedString(@"Theme setting for game %@", nil), title];
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
    ThemeArrayController *arrayController = _arrayController;
    Theme *themeToRemove = arrayController.selectedTheme;
    if (!themeToRemove.editable) {
        NSBeep();
        return;
    }
    if (themeToRemove.hardLightOrDark) {
        if (themeToRemove.hardDark) {
            self.darkTheme = nil;
            if (_darkOverrideActive) {
                [self lightOrDarkOverrideWasRemoved];
            }
        } else {
            self.lightTheme = nil;
            if (_lightOverrideActive) {
                [self lightOrDarkOverrideWasRemoved];
            }
        }
    }
    Theme *ancestor = themeToRemove.defaultParent;
    if (!ancestor)
        ancestor = themeToRemove.ancestor;
    NSSet *orphanedGames = themeToRemove.games;
    NSSet *orphanedThemes = themeToRemove.defaultChild;
    NSUInteger row = arrayController.selectionIndex;
    row = (row == NSNotFound || row == 0) ? 0 : row - 1;
    if (pendingSelectionTheme == themeToRemove)
        pendingSelectionTheme = nil;
    [arrayController removeObject:themeToRemove];
    // Select the theme this one was cloned or created from, if it still
    // exists. Otherwise the theme above the removed one.
    if (ancestor && [arrayController.arrangedObjects containsObject:ancestor])
        arrayController.selectedObjects = @[ancestor];
    else
        arrayController.selectionIndex = row;
    if (!ancestor)
        ancestor = arrayController.selectedTheme;

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
        if (!survivor || survivor.editable)
            survivor = self.defaultTheme;
    }

    NSMutableSet *orphanedGames = [[NSMutableSet alloc] init];

    for (Theme *t in fetchedObjects) {
        if (t.games.count || t.defaultChild.count) {
            Theme *ancestor = t.ancestor;
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
    [self findDarkAndLightThemes];
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

// Applies the Use in light/dark mode checkbox of mode to the selected theme.
// Switching one on switches the other off and takes the mode from the theme
// that had it.
- (void)changeHardThemeForMode:(kModeType)mode {
    BOOL dark = (mode == kDarkMode);
    NSButton *checkbox = dark ? _hardDarkCheckbox : _hardLightCheckbox;
    NSButton *otherCheckbox = dark ? _hardLightCheckbox : _hardDarkCheckbox;
    NSString *key = dark ? @"hardDark" : @"hardLight";
    NSString *otherKey = dark ? @"hardLight" : @"hardDark";
    Theme * __strong *modeTheme = dark ? &_darkTheme : &_lightTheme;
    Theme * __strong *otherModeTheme = dark ? &_lightTheme : &_darkTheme;
    BOOL modeOverrideActive = dark ? _darkOverrideActive : _lightOverrideActive;
    BOOL otherOverrideActive = dark ? _lightOverrideActive : _darkOverrideActive;

    Theme *theme = self.theme;
    BOOL lightOrDarkWasRemoved = NO;
    BOOL on = (checkbox.state == NSControlStateValueOn);
    [theme setValue:@(on) forKey:key];
    if (on) {
        otherCheckbox.state = NSControlStateValueOff;
        // If this was the theme of the other mode, nil it
        if (*otherModeTheme == theme) {
            *otherModeTheme = nil;
            if (otherOverrideActive)
                lightOrDarkWasRemoved = YES;
        }
        [theme setValue:@NO forKey:otherKey];
        if (*modeTheme && *modeTheme != theme) {
            // Reset any previous theme of this mode
            [*modeTheme setValue:@NO forKey:key];
            (*modeTheme).hardLightOrDark = NO;
        }
        theme.hardLightOrDark = YES;
        *modeTheme = theme;
    } else {
        // The checkbox was switched off, so both checkboxes are off
        theme.hardLightOrDark = NO;
        if (*modeTheme == theme) {
            *modeTheme = nil;
            if (modeOverrideActive)
                lightOrDarkWasRemoved = YES;
        }
    }

    if (lightOrDarkWasRemoved) {
        [self lightOrDarkOverrideWasRemoved];
    } else {
        [self noteColorModeChanged:nil];
    }
}

- (IBAction)changeHardDarkTheme:(id)sender {
    [self changeHardThemeForMode:kDarkMode];
}

- (IBAction)changeHardLightTheme:(id)sender {
    [self changeHardThemeForMode:kLightMode];
}

@end

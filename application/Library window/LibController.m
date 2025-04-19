/*
 *
 */

#import <BlorbFramework/Blorb.h>

#import "AppDelegate.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "TableViewController.h"

#import "LibController.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface MyButtonToolbarItem : NSToolbarItem
@end

@implementation MyButtonToolbarItem

- (void)validate {
    // validate content view
    NSControl *control = (NSControl *)self.view;
    SEL action = self.action;
    if (control && action) {
        id validator = [NSApp targetForAction:action to:self.target from:self];
        BOOL enabled;
        if ([validator conformsToProtocol:@protocol(NSUserInterfaceValidations)]) {
            enabled = [validator validateUserInterfaceItem:self];
        } else {
            enabled = [validator validateToolbarItem:self];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            control.enabled = enabled;
            control.needsDisplay = YES;
        });
    } else {
        [super validate];
    }
}

@end

@implementation WindowViewController
@end

@interface LibController () {
    NSToolbarItemIdentifier addToLibrary;
    NSToolbarItemIdentifier showInfo;
    NSToolbarItemIdentifier playGame;
    NSToolbarItemIdentifier searchBar;
}

@end

@implementation LibController

#pragma mark Window controller, menus and file dialogs.

- (void)windowDidLoad {
    self.windowFrameAutosaveName = @"LibraryWindow";
    self.window.acceptsMouseMovedEvents = YES;
    self.window.excludedFromWindowsMenu = YES;
    [self.window registerForDraggedTypes:@[ NSFilenamesPboardType ]];

    addToLibrary = @"addToLibrary";
    showInfo = @"showInfo";
    playGame = @"playGame";
    searchBar = @"searchBar";

    // This view controller determines the window toolbar's content.
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"toolbar"];
    toolbar.autosavesConfiguration = YES;
    toolbar.delegate = self;
    toolbar.displayMode = NSToolbarDisplayModeIconOnly;
    toolbar.allowsUserCustomization = NO;
    if (@available(macOS 11.0, *)) {
        self.window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
    }
    self.window.toolbar = toolbar;
}

#pragma mark Drag-n-drop destination handler

- (NSDragOperation)draggingEntered:sender {
    return [self.tableViewController draggingEntered:sender];
}

- (void)draggingExited:sender {
    // unhighlight window
}

- (BOOL)prepareForDragOperation:sender {
    return [self.tableViewController prepareForDragOperation:sender];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    return [self.tableViewController performDragOperation:sender];
}

- (NSManagedObjectContext *)managedObjectContext {
    if (_managedObjectContext == nil) {
        _managedObjectContext = ((AppDelegate*)NSApp.delegate).coreDataManager.mainManagedObjectContext;
    }
    return _managedObjectContext;
}

- (TableViewController *)tableViewController {
    if (_tableViewController == nil) {
        _tableViewController = ((AppDelegate*)NSApp.delegate).tableViewController;
    }
    return _tableViewController;
}

- (NSProgressIndicator *)progIndicator {
    if (_progIndicator == nil) {
        _progIndicator = ((WindowViewController *)self.window.contentViewController).progIndicator;
    }
    return _progIndicator;
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"HasAskedToDownload"] == NO) {
        if (_tableViewController)
            [_tableViewController askToDownload];
    }
    if (self.window.firstResponder == self.window)
        [self.window makeFirstResponder:_tableViewController.gameTableView];
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return self.managedObjectContext.undoManager;
}

- (MyButtonToolbarItem *)buttonToolbarItemWithImage:(NSString *)imageName selector:(SEL)selector label:(NSString *)label tooltip:(NSString *)tooltip identifier:(NSToolbarItemIdentifier)identifier enabled:(BOOL)enabled {
    NSImage *image = [NSImage imageNamed:imageName];
    image.accessibilityDescription = NSLocalizedString(tooltip, nil);
    NSSegmentedControl *button = [NSSegmentedControl segmentedControlWithImages:@[image] trackingMode:NSSegmentSwitchTrackingMomentary target: self.tableViewController action:selector];
    button.enabled = enabled;
    MyButtonToolbarItem *toolbarItem = [[MyButtonToolbarItem alloc] initWithItemIdentifier:identifier];
    toolbarItem.view = button;
    toolbarItem.label = NSLocalizedString(label, nil);
    toolbarItem.toolTip = NSLocalizedString(tooltip, nil);
    return toolbarItem;
}

- (NSToolbarItem *)toolbarItemWithImage:(NSString *)imageName selector:(SEL)selector label:(NSString *)label tooltip:(NSString *)tooltip identifier:(NSToolbarItemIdentifier)identifier enabled:(BOOL)enabled API_AVAILABLE(macos(11.0)){
    NSToolbarItem *toolbarItem = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
    NSImageSymbolConfiguration *config = [NSImageSymbolConfiguration configurationWithScale:NSImageSymbolScaleLarge];
    NSImage *image = [NSImage imageWithSystemSymbolName:imageName accessibilityDescription:tooltip];
    image = [image imageWithSymbolConfiguration:config];
    toolbarItem.image = image;
    toolbarItem.action = selector;
    toolbarItem.target = self.tableViewController;
    toolbarItem.bordered = YES;
    toolbarItem.label = NSLocalizedString(label, nil);
    toolbarItem.toolTip = NSLocalizedString(tooltip, nil);
    toolbarItem.enabled = enabled;
    return toolbarItem;
}


- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    NSToolbarItem *toolbarItem = nil;

    if (itemIdentifier == addToLibrary) {
        if (@available(macOS 11.0, *)) {
            return [self toolbarItemWithImage:@"plus.circle" selector:@selector(addGamesToLibrary:) label:@"Add" tooltip:@"Add games to library" identifier:itemIdentifier enabled:YES];
        } else {
            return [self buttonToolbarItemWithImage:@"plus" selector:@selector(addGamesToLibrary:) label:@"Add" tooltip:@"Add games to library" identifier:itemIdentifier enabled:YES];
        }
    } else if (itemIdentifier == showInfo) {
        if (@available(macOS 11.0, *)) {
            return [self toolbarItemWithImage:@"eye.circle" selector:@selector(showGameInfo:) label:@"Show info" tooltip:@"Show info for selected game" identifier:itemIdentifier enabled:NO];
        } else {
            return [self buttonToolbarItemWithImage:@"eye.fill" selector:@selector(showGameInfo:) label:@"Show Info" tooltip:@"Show info for selected game" identifier:itemIdentifier enabled:NO];
        }
    } else if (itemIdentifier == playGame) {
        if (@available(macOS 11.0, *)) {
            return [self toolbarItemWithImage:@"play.circle" selector:@selector(play:) label:@"Play" tooltip:@"Play selected game" identifier:itemIdentifier enabled:NO];
        } else {
            return [self buttonToolbarItemWithImage:@"play.fill" selector:@selector(play:) label:@"Play" tooltip:@"Play selected game" identifier:itemIdentifier enabled:NO];
        }
    } else if (itemIdentifier == searchBar) {
        NSSearchField *searchField = [[NSSearchField alloc] initWithFrame: CGRectZero];
        _searchField = searchField;
        searchField.action = @selector(searchForGames:);
        searchField.target = self.tableViewController;
        toolbarItem = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        toolbarItem.view = searchField;
        toolbarItem.label = NSLocalizedString(@"Search", nil);
        toolbarItem.toolTip = NSLocalizedString(@"Search game database", nil);

        if (@available(macOS 11.0, *)) {
        } else {
            toolbarItem.maxSize = NSMakeSize(200, 20);
            toolbarItem.minSize = NSMakeSize(100, 20);
        }
    }
    return toolbarItem;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {

    NSMutableArray<NSToolbarItemIdentifier> *toolbarItemIdentifiers = [NSMutableArray new];

    [toolbarItemIdentifiers addObject:NSToolbarToggleSidebarItemIdentifier];

    [toolbarItemIdentifiers addObject:addToLibrary];
    [toolbarItemIdentifiers addObject:showInfo];
    [toolbarItemIdentifiers addObject:playGame];

    if (@available(macOS 11.0, *)) {
    } else {
        [toolbarItemIdentifiers addObject:NSToolbarFlexibleSpaceItemIdentifier];
    }

    [toolbarItemIdentifiers addObject:searchBar];

    return toolbarItemIdentifiers;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return [self toolbarDefaultItemIdentifiers:toolbar];
}

#pragma mark -
#pragma mark Windows restoration

- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state {
    [state encodeObject:_searchField.stringValue forKey:@"searchText"];
    [state encodeObject:@(self.tableViewController.gameTableView.enclosingScrollView.contentView.bounds.origin.y) forKey:@"scrollPosition"];
    if (self.tableViewController.selectedGames.count) {
        NSMutableArray *selectedGameIfids = [NSMutableArray arrayWithCapacity:_tableViewController.selectedGames.count];
        NSString *str;
        for (Game *game in _tableViewController.selectedGames) {
            str = game.ifid;
            if (str) {
                [selectedGameIfids addObject:str];
            }
        }
        [state encodeObject:selectedGameIfids forKey:@"selectedGames"];
    }
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    NSString *searchText = [state decodeObjectOfClass:[NSString class] forKey:@"searchText"];
    if (searchText.length) {
        _searchField.stringValue = searchText;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
            [self.tableViewController searchForGames:self.searchField];
        });
    }
    NSNumber *scrollPosNum = [state decodeObjectOfClass:[NSNumber class] forKey:@"scrollPosition"];
    CGFloat scrollPosition = scrollPosNum.floatValue;
    NSArray *selectedIfids;
    if (@available(macOS 11.0, *)) {
        @try {
            selectedIfids = [state decodeArrayOfObjectsOfClasses:[NSSet setWithObject:[NSString class]] forKey:@"selectedGames"];
        } @catch (NSException *exception) {
            selectedIfids = [state decodeObjectOfClass:[NSArray class] forKey:@"selectedGames"];
        }
    } else {
        selectedIfids = [state decodeObjectOfClass:[NSArray class] forKey:@"selectedGames"];
    }

    [self.tableViewController updateTableViews];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [self.tableViewController selectGamesWithIfids:selectedIfids scroll:NO];
        NSRect bounds = self.tableViewController.gameTableView.enclosingScrollView.contentView.bounds;
        bounds.origin.y = scrollPosition;
        self.tableViewController.gameTableView.enclosingScrollView.contentView.bounds = bounds;
    });
}
@end

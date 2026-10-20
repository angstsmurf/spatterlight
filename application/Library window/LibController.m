/*
 *
 */

#import "AppDelegate.h"
#import "CoreDataManager.h"
#import "Game.h"
#import "TableViewController.h"
#import "TableViewController+DragDrop.h"
#import "TableViewController+GameActions.h"
#import "TableViewController+InfoWindows.h"
#import "TableViewController+TableDelegate.h"
#import "MetadataHandler.h"
#import "LibHelperTableView.h"

#import "LibController.h"

#import "DebugLog.h"

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

static NSToolbarItemIdentifier const kSearchBarItem = @"searchBar";

// The toolbar buttons. -validateToolbarItem: in TableViewController+GameActions.m
// matches these identifiers by value.
typedef struct {
    NSToolbarItemIdentifier __unsafe_unretained identifier;
    NSString * __unsafe_unretained symbolName;  // macOS 11+: SF Symbol
    NSString * __unsafe_unretained imageName;   // macOS 10.x: asset image
    NSString * __unsafe_unretained action;      // selector name; @selector() is not a constant
    NSString * __unsafe_unretained label;
    NSString * __unsafe_unretained tooltip;
    BOOL enabled;
} ButtonItemSpec;

static const ButtonItemSpec kButtonItems[] = {
    { @"addToLibrary", @"plus.circle", @"plus",     @"addGamesToLibrary:", @"Add",       @"Add games to library",        YES },
    { @"showInfo",     @"eye.circle",  @"eye.fill", @"showGameInfo:",      @"Show info", @"Show info for selected game", NO },
    { @"playGame",     @"play.circle", @"play.fill", @"play:",              @"Play",      @"Play selected game",          NO },
};

@implementation LibController

#pragma mark Window controller, menus and file dialogs.

- (void)windowDidLoad {
    self.windowFrameAutosaveName = @"LibraryWindow";
    self.window.acceptsMouseMovedEvents = YES;
    self.window.excludedFromWindowsMenu = YES;
    [self.window registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];

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
            [_tableViewController.metadataHandler askToDownloadInWindow:self.window context:self.managedObjectContext];
    }
    if (self.window.firstResponder == self.window)
        [self.window makeFirstResponder:_tableViewController.gameTableView];
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return self.managedObjectContext.undoManager;
}

- (NSToolbarItem *)toolbarItemForSpec:(const ButtonItemSpec *)spec {
    NSString *tooltip = NSLocalizedString(spec->tooltip, nil);
    NSToolbarItem *toolbarItem;
    if (@available(macOS 11.0, *)) {
        toolbarItem = [[NSToolbarItem alloc] initWithItemIdentifier:spec->identifier];
        NSImageSymbolConfiguration *config = [NSImageSymbolConfiguration configurationWithScale:NSImageSymbolScaleLarge];
        NSImage *image = [NSImage imageWithSystemSymbolName:spec->symbolName accessibilityDescription:tooltip];
        toolbarItem.image = [image imageWithSymbolConfiguration:config];
        toolbarItem.action = NSSelectorFromString(spec->action);
        toolbarItem.target = self.tableViewController;
        toolbarItem.bordered = YES;
        toolbarItem.enabled = spec->enabled;
    } else {
        NSImage *image = [NSImage imageNamed:spec->imageName];
        image.accessibilityDescription = tooltip;
        NSSegmentedControl *button = [NSSegmentedControl segmentedControlWithImages:@[image] trackingMode:NSSegmentSwitchTrackingMomentary target:self.tableViewController action:NSSelectorFromString(spec->action)];
        button.enabled = spec->enabled;
        toolbarItem = [[MyButtonToolbarItem alloc] initWithItemIdentifier:spec->identifier];
        toolbarItem.view = button;
    }
    toolbarItem.label = NSLocalizedString(spec->label, nil);
    toolbarItem.toolTip = tooltip;
    return toolbarItem;
}

- (NSSearchField *)configureSearchField:(NSSearchField *)field {
    field.action = @selector(searchForGames:);
    field.target = self.tableViewController;
    // Keep the existing weak-ish reference used elsewhere (state restore, etc.)
    _searchField = field;
    // Stop AppKit's autofill heuristic from walking outward into the
    // library window's content hierarchy. AppKit re-links the field's
    // key-view neighbours after the toolbar lays out, so we re-assert
    // this self-loop on focus in -controlTextDidBeginEditing: as well.
    field.delegate = self;
    [field setNextKeyView:field];
    return field;
}

- (NSToolbarItem *)searchToolbarItem {
    NSToolbarItem *toolbarItem;
    if (@available(macOS 11.0, *)) {
        NSSearchToolbarItem *searchItem =
        [[NSSearchToolbarItem alloc] initWithItemIdentifier:kSearchBarItem];
        // Configure the built-in search field. Don't replace it — just configure it.
        [self configureSearchField:searchItem.searchField];
        searchItem.resignsFirstResponderWithCancel = YES;
        searchItem.preferredWidthForSearchField = 100;
        toolbarItem = searchItem;
    } else {
        // Pre-11 fallback: cache & reuse so we don't leak a dangling field
        // into the window's key-view chain.
        if (!_searchField)
            [self configureSearchField:[[NSSearchField alloc] initWithFrame:CGRectZero]];
        toolbarItem = [[NSToolbarItem alloc] initWithItemIdentifier:kSearchBarItem];
        toolbarItem.view    = _searchField;
        toolbarItem.maxSize = NSMakeSize(200, 20);
        toolbarItem.minSize = NSMakeSize(100, 20);
    }
    toolbarItem.label   = NSLocalizedString(@"Search", nil);
    toolbarItem.toolTip = NSLocalizedString(@"Search game database", nil);
    return toolbarItem;
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    if ([itemIdentifier isEqualToString:kSearchBarItem])
        return [self searchToolbarItem];
    for (size_t i = 0; i < sizeof(kButtonItems) / sizeof(kButtonItems[0]); i++) {
        if ([itemIdentifier isEqualToString:kButtonItems[i].identifier])
            return [self toolbarItemForSpec:&kButtonItems[i]];
    }
    return nil;
}

// When the search field gains focus, AppKit schedules its password-autofill
// heuristic (NSAutoFillHeuristicController) as a run-loop block. That block
// walks the key-view loop outward via -previousValidKeyView; if the walk
// reaches a neighbouring view that can't handle the message it sends, the app
// traps in CoreFoundation's forwarding path. This notification fires
// synchronously on focus -- before that deferred block runs -- so re-asserting
// the self-contained key-view loop here keeps the walk from ever leaving the
// field, even after AppKit re-links neighbours during toolbar layout.
- (void)controlTextDidBeginEditing:(NSNotification *)obj {
    if (obj.object == _searchField) {
        [_searchField setNextKeyView:_searchField];
    }
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    NSMutableArray<NSToolbarItemIdentifier> *toolbarItemIdentifiers =
    [NSMutableArray arrayWithObject:NSToolbarToggleSidebarItemIdentifier];
    for (size_t i = 0; i < sizeof(kButtonItems) / sizeof(kButtonItems[0]); i++)
        [toolbarItemIdentifiers addObject:kButtonItems[i].identifier];

    // macOS 11+ pushes the search item to the trailing edge by itself.
    if (@available(macOS 11.0, *)) {
    } else {
        [toolbarItemIdentifiers addObject:NSToolbarFlexibleSpaceItemIdentifier];
    }

    [toolbarItemIdentifiers addObject:kSearchBarItem];
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
        NSMutableArray *selectedGameHashtags = [NSMutableArray arrayWithCapacity:_tableViewController.selectedGames.count];
        NSString *str;
        for (Game *game in _tableViewController.selectedGames) {
            str = game.hashTag;
            if (str) {
                [selectedGameHashtags addObject:str];
            }
        }
        [state encodeObject:selectedGameHashtags forKey:@"selectedGames"];
    }
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    if ([TableViewController launchedWithTestGames])
        return;
    NSString *searchText = [state decodeObjectOfClass:[NSString class] forKey:@"searchText"];
    if (searchText.length) {
        _searchField.stringValue = searchText;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
            [self.tableViewController searchForGames:self.searchField];
        });
    }
    NSNumber *scrollPosNum = [state decodeObjectOfClass:[NSNumber class] forKey:@"scrollPosition"];
    CGFloat scrollPosition = scrollPosNum.doubleValue;
    NSArray *selectedHashtags;
    if (@available(macOS 11.0, *)) {
        @try {
            selectedHashtags = [state decodeArrayOfObjectsOfClasses:[NSSet setWithObject:[NSString class]] forKey:@"selectedGames"];
        } @catch (NSException *exception) {
            selectedHashtags = [state decodeObjectOfClass:[NSArray class] forKey:@"selectedGames"];
        }
    } else {
        selectedHashtags = [state decodeObjectOfClass:[NSArray class] forKey:@"selectedGames"];
    }

    [self.tableViewController updateTableViews];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        [self.tableViewController selectGamesWithHashes:selectedHashtags scroll:YES];
        NSRect bounds = self.tableViewController.gameTableView.enclosingScrollView.contentView.bounds;
        bounds.origin.y = scrollPosition;
        self.tableViewController.gameTableView.enclosingScrollView.contentView.bounds = bounds;
    });
}
@end

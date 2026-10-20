//
//  TableViewController+TableDelegate.m
//  Spatterlight
//
//  Table view data source/delegate, sorting, searching, editing, selection.
//

#import "TableViewController+TableDelegate.h"
#import "TableViewController+Internal.h"
#import "TableViewController+GameActions.h"

#import "LibHelperTableView.h"

#import "LibController.h"
#import "GlkController.h"
#import "Preferences.h"
#import "Constants.h"

#import "NSString+Categories.h"
#import "NSDate+relative.h"

#import "Game.h"
#import "Metadata.h"
#import "Theme.h"
#import "Fetches.h"

#import "GameLauncher.h"
#import "LibraryOrganizer.h"

@implementation TableViewController (TableDelegate)

#pragma mark -
#pragma mark Table magic

- (IBAction)searchForGames:(nullable id)sender {
    self.searchString = [[sender stringValue] stringByReplacingOccurrencesOfString:@"\"" withString:@""];
    self.gameTableDirty = YES;
    [self updateTableViews];
}

- (IBAction)toggleColumn:(id)sender {
    NSMenuItem *item = (NSMenuItem *)sender;
    NSTableColumn *column = [self.gameTableView tableColumnWithIdentifier:item.identifier];
    BOOL hide = (item.state == NSControlStateValueOn);
    column.hidden = hide;
    item.state = hide ? NSControlStateValueOff : NSControlStateValueOn;
}

- (NSRect)rectForLineWithHash:(NSString*)hashTag {
    Game *game;
    NSRect frame = NSZeroRect;
    NSRect myFrame = self.view.window.frame;
    frame.origin.x = myFrame.origin.x + myFrame.size.width / 2;
    frame.origin.y = myFrame.origin.y + myFrame.size.height / 2;

    if (hashTag.length) {
        game = [Fetches fetchGameForHash:hashTag inContext:self.managedObjectContext];
        if ([self.gameTableModel containsObject:game]) {
            NSUInteger index = [self.gameTableModel indexOfObject:game];
            frame = [self.gameTableView rectOfRow:(NSInteger)index];
            frame = [self.gameTableView convertRect:frame toView:nil];
            frame = [self.view.window convertRectToScreen:frame];
        }
    }
    frame.origin.x += 12;
    frame.size.width -= 24;
    return frame;
}

- (void)selectGamesWithHashes:(NSArray*)hashes scroll:(BOOL)shouldscroll {
    [self selectGamesWithHashes:hashes scroll:shouldscroll clearSearchIfHidden:NO];
}

- (void)selectGamesWithHashes:(NSArray*)hashes scroll:(BOOL)shouldscroll clearSearchIfHidden:(BOOL)clearSearch {
    if (hashes.count) {
        NSMutableArray *newSelection = [NSMutableArray arrayWithCapacity:hashes.count];
        for (NSString *hashTag in hashes) {
            Game *game = [Fetches fetchGameForHash:hashTag inContext:self.managedObjectContext];
            if (game) {
                [newSelection addObject:game];
            } else {
                NSLog(@"No game with hash %@ in library, cannot restore selection", hashTag);
            }
        }

        // Keep the current selection rather than clearing it when none of
        // the games are left, e.g. when the window restores a selection of
        // games that have since been deleted.
        if (!newSelection.count)
            return;

        // If any of the games we want to select are filtered out by the
        // current search string, clear the search bar so they become
        // visible. This rebuilds gameTableModel synchronously, so the
        // membership check below will then find them.
        if (clearSearch && self.searchString.length) {
            for (Game *game in newSelection) {
                if (![self.gameTableModel containsObject:game]) {
                    self.windowController.searchField.stringValue = @"";
                    [self searchForGames:nil];
                    break;
                }
            }
        }

        NSMutableIndexSet *indexSet = [NSMutableIndexSet indexSet];

        for (Game *game in newSelection) {
            if ([self.gameTableModel containsObject:game]) {
                [indexSet addIndex:[self.gameTableModel indexOfObject:game]];
            }
        }
        [self.gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        self.selectedGames = [self.gameTableModel objectsAtIndexes:indexSet];
        if (shouldscroll && indexSet.count && !self.currentlyAddingGames) {
            [self.gameTableView scrollRowToVisible:(NSInteger)indexSet.firstIndex];
        }
    }
}

- (void)selectGames:(NSSet*)games
{
    if (games.count) {
        NSIndexSet *indexSet = [self.gameTableModel indexesOfObjectsPassingTest:^BOOL(Game *obj, NSUInteger idx, BOOL *stop) {
            return [games containsObject:obj];
        }];
        [self.gameTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        self.selectedGames = [self.gameTableModel objectsAtIndexes:indexSet];
    }
}

#pragma mark Sorting helpers

static NSInteger compareTitleStrings(NSString *a, NSString *b) {
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedStandardCompare: b];
}

// Empty values sort last, whichever the direction.
static NSInteger compareStrings(NSString *a, NSString *b, BOOL ascending) {
    if (a.length == 0 && b.length == 0)
        return NSOrderedSame;
    if (a.length == 0) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (b.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;

    return compareTitleStrings(a, b);
}

static NSInteger compareMetadataKey(Metadata *a, Metadata *b, NSString *key, BOOL ascending) {
    return compareStrings([a valueForKey:key], [b valueForKey:key], ascending);
}

static NSInteger compareDates(NSDate *a, NSDate *b, BOOL ascending) {
    if (!a && !b)
        return NSOrderedSame;
    if (!a) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!b) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [a compare:b];
}

// NSNotFound sorts last, whichever the direction.
static NSInteger compareIntegers(NSInteger a, NSInteger b, BOOL ascending) {
    if (a == b)
        return NSOrderedSame;
    if (a == NSNotFound) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (b == NSNotFound) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return (a < b) ? NSOrderedAscending : NSOrderedDescending;
}

static NSInteger seriesNumber(Metadata *meta) {
    return meta.seriesnumber ? meta.seriesnumber.integerValue : NSNotFound;
}

// Game.like is 0 (neither), 1 (liked) or 2 (disliked). Sort disliked
// between the two, so that liked games come first in descending order.
static NSInteger likeRank(NSInteger like) {
    if (like == 1)
        return 2;
    if (like == 2)
        return 1;
    return like;
}

static NSPredicate *searchPredicateForWord(NSString *word) {
    static NSArray<NSString *> *keyPaths;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        keyPaths = @[ @"detectedFormat", @"metadata.title", @"metadata.author", @"group",
                      @"metadata.genre", @"metadata.series", @"metadata.seriesnumber",
                      @"metadata.forgiveness", @"metadata.languageAsWord" ];
    });
    NSMutableArray<NSPredicate *> *predicates = [NSMutableArray arrayWithCapacity:keyPaths.count + 1];
    for (NSString *keyPath in keyPaths)
        [predicates addObject:[NSPredicate predicateWithFormat:@"%K contains[c] %@", keyPath, word]];
    [predicates addObject:[NSPredicate predicateWithFormat:@"metadata.firstpublished contains %@", word]];
    return [NSCompoundPredicate orPredicateWithSubpredicates:predicates];
}

//! Orders two games by the sort column, then by title, author, series
//! number, year and finally hash, so that the order is always the same.
- (NSInteger)compareGame:(Game *)aid with:(Game *)bid {
    NSString *column = self.gameSortColumn;
    BOOL ascending = self.sortAscending;
    Metadata *a = aid.metadata;
    Metadata *b = bid.metadata;
    NSInteger cmp = NSOrderedSame;

    if ([column isEqual:@"firstpublishedDate"]) {
        cmp = compareDates(a.firstpublishedDate, b.firstpublishedDate, ascending);
    } else if ([column isEqual:@"added"] || [column isEqual:@"lastPlayed"]) {
        cmp = compareDates([aid valueForKey:column], [bid valueForKey:column], ascending);
    } else if ([column isEqual:@"lastModified"]) {
        cmp = compareDates(a.lastModified, b.lastModified, ascending);
    } else if ([column isEqual:@"found"]) {
        NSString *string1, *string2;
        [self imageNameForGame:aid description:&string1];
        [self imageNameForGame:bid description:&string2];
        cmp = compareStrings(string1, string2, ascending);
    } else if ([column isEqual:@"like"]) {
        cmp = compareIntegers(likeRank(aid.like), likeRank(bid.like), ascending);
    } else if ([column isEqual:@"forgivenessNumeric"]) {
        cmp = compareStrings(a.forgivenessNumeric.stringValue, b.forgivenessNumeric.stringValue, ascending);
    } else if ([column isEqual:@"seriesnumber"]) {
        cmp = compareIntegers(seriesNumber(a), seriesNumber(b), ascending);
    } else if ([column isEqual:@"series"]) {
        cmp = compareMetadataKey(a, b, column, ascending);
        if (cmp == NSOrderedSame)
            cmp = compareIntegers(seriesNumber(a), seriesNumber(b), ascending);
    } else if ([column isEqual:@"format"]) {
        cmp = compareStrings(aid.detectedFormat, bid.detectedFormat, ascending);
    } else if ([column isEqual:@"group"]) {
        cmp = compareStrings(aid.group, bid.group, ascending);
    } else if (column) {
        cmp = compareMetadataKey(a, b, column, ascending);
    }
    if (cmp) return cmp;

    cmp = compareMetadataKey(a, b, @"title", ascending);
    if (cmp) return cmp;
    cmp = compareMetadataKey(a, b, @"author", ascending);
    if (cmp) return cmp;
    cmp = compareMetadataKey(a, b, @"seriesnumber", ascending);
    if (cmp) return cmp;
    cmp = compareDates(a.firstpublishedDate, b.firstpublishedDate, ascending);
    if (cmp) return cmp;
    return (NSInteger)[aid.hashTag compare:bid.hashTag];
}

#pragma mark Update table views

- (void)updateTableViews {
    if (!self.gameTableDirty)
        return;

    NSError *error = nil;
    NSArray<Game *> *searchResult = nil;

    NSFetchRequest *fetchRequest = [Game fetchRequest];
    NSMutableArray<NSPredicate *> *predicateArray = [NSMutableArray new];

    [predicateArray addObject:[NSPredicate predicateWithFormat:@"hidden == NO"]];

    BOOL alreadySearched = NO;

    if (self.searchString.length)
    {
        // First we search using the entire search string as a phrase
        [predicateArray addObject:searchPredicateForWord(self.searchString)];

        fetchRequest.predicate = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];
        searchResult = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
        if (error)
            NSLog(@"executeFetchRequest for searchString: %@", error);

        alreadySearched = YES;

        // If this gives zero results, search for each word separately
        if (searchResult.count == 0) {
            NSArray *searchStrings = [self.searchString componentsSeparatedByString:@" "];
            if (searchStrings.count > 1) {
                alreadySearched = NO;
                [predicateArray removeLastObject];

                for (NSString *word in searchStrings) {
                    if (word.length)
                        [predicateArray addObject:searchPredicateForWord(word)];
                }
            }
        }
    }

    if (!searchResult.count && !alreadySearched) {
        fetchRequest.predicate = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];
        error = nil;
        searchResult = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    }

    if (searchResult == nil) {
        NSLog(@"Problem! %@",error);
        return;
    }

    TableViewController * __weak weakSelf = self;

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:self.sortAscending comparator:^(Game *a, Game *b) {
        return [weakSelf compareGame:a with:b];
    }];

    NSSortDescriptor *fallback = [NSSortDescriptor sortDescriptorWithKey:@"hashTag" ascending:self.sortAscending comparator:^(NSString *a, NSString *b) {
        return [a compare:b];
    }];

    // Replace the model, sort it and reload the table in one go. Every
    // action that maps a row of the table to a game (play:, the star rating,
    // the like button, the contextual menu) indexes gameTableModel, so the
    // model must never hold an order the table is not showing.
    NSMutableArray<Game *> *model = searchResult.mutableCopy;
    [model sortUsingDescriptors:@[sort, fallback]];
    self.gameTableModel = model;
    self.gameTableDirty = NO;

    NSTableView *tableView = self.gameTableView;
    if ([model isEqualToArray:self.displayedGames] &&
        tableView.numberOfRows == (NSInteger)model.count) {
        // The same games in the same order, which is what most Core Data
        // changes amount to (starting a game stamps its lastPlayed date,
        // for one). Refresh the cells in place: a full reload throws away
        // and rebuilds every visible row, which stalls the main thread
        // long enough to delay a game that is starting up.
        // Leave a row that is being renamed alone: refreshing it drops
        // the cell under the field editor, and the recycled cell can
        // come back on a neighbouring row, where the edit then commits.
        NSMutableIndexSet *rows = [NSMutableIndexSet indexSetWithIndexesInRange:NSMakeRange(0, model.count)];
        NSInteger edited = tableView.editedRow;
        if (edited >= 0)
            [rows removeIndex:(NSUInteger)edited];
        [tableView reloadDataForRowIndexes:rows
                             columnIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, (NSUInteger)tableView.numberOfColumns)]];
    } else {
        [tableView reloadData];
    }
    self.displayedGames = [model copy];
    [self selectGames:[NSSet setWithArray:self.selectedGames]];
    [self invalidateRestorableState];
}

- (void)updateWindowSubtitle {
    if (@available(macOS 11.0, *)) {
        NSFetchRequest *fetchRequest = [Game fetchRequest];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hidden == NO"];
        NSError *error = nil;
        NSUInteger count = [self.managedObjectContext countForFetchRequest:fetchRequest error:&error];
        if (count == NSNotFound) {
            NSLog(@"updateWindowSubtitle: %@", error);
            return;
        }
        if (count == 0)
            self.view.window.subtitle = NSLocalizedString(@"No games", nil);
        else if (count == 1)
            self.view.window.subtitle = NSLocalizedString(@"1 game", nil);
        else
            self.view.window.subtitle = [NSString stringWithFormat:NSLocalizedString(@"%lu games", nil), (unsigned long)count];
    }
}

#pragma mark NSTableViewDelegate / DataSource

// An SF Symbol on macOS 11 and later, an image of the same name from the
// asset catalog before that.
static NSImage *symbolImage(NSString *name, NSString *description) {
    if (!name)
        return nil;
    NSImage *image;
    if (@available(macOS 11.0, *)) {
        image = [NSImage imageWithSystemSymbolName:name accessibilityDescription:description];
    } else {
        image = [NSImage imageNamed:name];
        image.accessibilityDescription = description;
    }
    return image;
}

- (void)tableView:(NSTableView *)tableView
sortDescriptorsDidChange:(NSArray *)oldDescriptors {
    if (tableView == self.gameTableView) {
        NSArray *sortDescriptors = tableView.sortDescriptors;
        if (!sortDescriptors.count)
            return;
        NSSortDescriptor *sortDescriptor = sortDescriptors[0];
        if (!sortDescriptor)
            return;
        self.gameSortColumn = sortDescriptor.key;
        self.sortAscending = sortDescriptor.ascending;
        self.gameTableDirty = YES;
        [self updateTableViews];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    if (tableView == self.gameTableView)
        return (NSInteger)self.gameTableModel.count;
    return 0;
}

// Pure: a game without a hash cannot have a session (they are keyed by
// hash), so this does not compute one. The cell code does that.
- (NSString *)imageNameForGame:(Game *)game description:(NSString * __autoreleasing *)description {
    NSString *name;
    if (!game.found) {
        *description = NSLocalizedString(@"File not found", nil);
        name = @"exclamationmark.circle";
    } else {
        BOOL playing = NO;
        GlkController *session = game.hashTag.length ? self.gameSessions[game.hashTag] : nil;
        if (session) {
            if (session.alive || session.showingCoverImage) {
                *description = NSLocalizedString(@"In progress", nil);
                if (@available(macOS 11.0, *)) {
                    name = @"play.fill";
                } else {
                    name = @"play";
                }
            } else {
                *description = NSLocalizedString(@"Stopped", nil);
                if (@available(macOS 11.0, *)) {
                    name = @"stop.fill";
                } else {
                    name = @"stop";
                }
            }
            playing = YES;
        }
        if (!playing) {
            if (game.autosaved) {
                *description = NSLocalizedString(@"Autosaved", nil);
                name = @"pause.fill";
            } else {
                *description = nil;
                name = nil;
            }
        }
    }
    return name;
}

//! Bold header text for the sort column, small for the like column
- (void)updateHeaderFontOfColumn:(NSTableColumn *)column {
    NSMutableAttributedString *headerAttrStr = column.headerCell.attributedStringValue.mutableCopy;

    if (!headerAttrStr.length)
        return;

    NSMutableDictionary *attributes = [headerAttrStr attributesAtIndex:0 effectiveRange:nil].mutableCopy;
    NSFont *font = nil;
    if ([self.gameSortColumn isEqualToString:column.identifier]) {
        font = [NSFont systemFontOfSize:12 weight:NSFontWeightBold];
    } else {
        font = [NSFont systemFontOfSize:12];
    }

    if ([column.identifier isEqual:@"like"])
        font = [NSFont systemFontOfSize:10];

    attributes[NSFontAttributeName] = font;
    column.headerCell.attributedStringValue = [[NSAttributedString alloc] initWithString:headerAttrStr.string attributes:attributes];
}

- (void)configureFoundCell:(NSTableCellView *)cellView game:(Game *)game {
    if (game.found && game.hashTag.length == 0) {
        game.hashTag = [game.path signatureFromFile];
        if (game.hashTag.length)
            game.metadata.hashTag = game.hashTag;
    }
    NSString *description;
    NSString *name = [self imageNameForGame:game description:&description];
    cellView.imageView.image = symbolImage(name, description);
    cellView.imageView.accessibilityLabel = description;
}

//! The heart of a game that is neither liked nor disliked shows only under
//! the mouse.
static NSImage *hoverLikeImage(BOOL hovered) {
    return hovered ? symbolImage(@"heart", NSLocalizedString(@"Like", nil)) : nil;
}

- (void)configureLikeCell:(LikeCellView *)likeCellView game:(Game *)game row:(NSInteger)row {
    switch (game.like) {
        case 2:
            likeCellView.likeButton.image = symbolImage(@"heart.slash.fill", NSLocalizedString(@"Disliked", nil));
            likeCellView.toolTip = NSLocalizedString(@"Disliked", nil);
            likeCellView.accessibilityLabel = NSLocalizedString(@"Disliked", nil);
            break;
        case 1:
            likeCellView.likeButton.image = symbolImage(@"heart.fill", NSLocalizedString(@"Liked", nil));
            likeCellView.toolTip = NSLocalizedString(@"Liked", nil);
            likeCellView.accessibilityLabel = NSLocalizedString(@"Liked", nil);
            break;
        default:
            likeCellView.likeButton.image = hoverLikeImage(row == self.gameTableView.mouseOverRow);
            likeCellView.toolTip = NSLocalizedString(@"Like", nil);
            likeCellView.accessibilityLabel = NSLocalizedString(@"Not liked", nil);
            break;
    }
}

- (void)configureForgivenessCell:(ForgivenessCellView *)forgivenessCell metadata:(Metadata *)meta {
    NSInteger value = meta.forgivenessNumeric.integerValue;
    if (value < 0 || value > 5)
        value = 0;
    NSPopUpButton *popUp = forgivenessCell.popUpButton;
    NSMenuItem *none = popUp.menu.itemArray[0];
    if (value == 0) {
        [popUp selectItem:none];
        none.state = NSControlStateValueOn;
        if (@available(macOS 10.14, *)) {
            popUp.title = @"";
        } else {
            none.title = @"  ";
            popUp.title = @"  ";
        }
    } else {
        [popUp selectItem:[popUp.menu itemWithTag:value]];
        popUp.title = meta.forgiveness;
    }
    popUp.cell.bordered = NO;
}

- (void)configureRatingCell:(RatingsCellView *)indicatorCell metadata:(Metadata *)meta isMyRating:(BOOL)isMy row:(NSInteger)row {
    NSInteger value;
    NSLevelIndicator *indicator = indicatorCell.rating;

    if (!isMy) {
        value = meta.starRating.integerValue;
        ((MyIndicator *)indicator).tableView = self.gameTableView;
    } else {
        value = meta.myRating.integerValue;
    }

    if (value > 0 || (isMy && row == self.gameTableView.mouseOverRow)) {
        indicator.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityAlways;
    } else {
        indicator.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityWhileEditing;
    }
    indicator.objectValue = @(value);
}

- (nullable NSView *)tableView:(NSTableView*)tableView viewForTableColumn:(nullable NSTableColumn *)column row:(NSInteger)row {

    NSTableCellView *cellView = [tableView makeViewWithIdentifier:column.identifier owner:self];

    if (tableView != self.gameTableView)
        return cellView;

    if ((NSUInteger)row >= self.gameTableModel.count) return nil;

    [self updateHeaderFontOfColumn:column];

    Game *game = self.gameTableModel[(NSUInteger)row];
    Metadata *meta = game.metadata;

    NSString *identifier = column.identifier;

    if ([identifier isEqual:@"found"]) {
        [self configureFoundCell:cellView game:game];
    } else if ([identifier isEqual:@"like"]) {
        [self configureLikeCell:(LikeCellView *)cellView game:game row:row];
    } else if ([identifier isEqual:@"forgivenessNumeric"]) {
        [self configureForgivenessCell:(ForgivenessCellView *)cellView metadata:meta];
    } else if ([identifier isEqual:@"starRating"] ||
               [identifier isEqual:@"myRating"]) {
        [self configureRatingCell:(RatingsCellView *)cellView metadata:meta isMyRating:[identifier isEqual:@"myRating"] row:row];
    } else {
        if ([identifier isEqual:@"format"] && !game.detectedFormat)
            game.detectedFormat = meta.format;
        NSString *string = [self stringForGame:game columnIdentifier:identifier] ?: @"";
        // Kept so endedEditing: can tell an edit from text left unchanged
        cellView.objectValue = string;
        cellView.textField.stringValue = string;
    }
    return cellView;
}

//! The text of a game's cell in a text column. Also used to size columns.
- (nullable NSString *)stringForGame:(Game *)game columnIdentifier:(NSString *)identifier {
    Metadata *meta = game.metadata;
    if ([identifier isEqual:@"format"])
        return game.detectedFormat ?: meta.format;
    if ([identifier isEqual:@"added"] || [identifier isEqual:@"lastPlayed"])
        return [[game valueForKey:identifier] formattedRelativeString];
    if ([identifier isEqual:@"lastModified"])
        return [meta.lastModified formattedRelativeString];
    if ([identifier isEqual:@"firstpublishedDate"]) {
        NSDate *date = meta.firstpublishedDate;
        if (!date)
            return nil;
        NSInteger year = [[NSCalendar currentCalendar] component:NSCalendarUnitYear fromDate:date];
        return [NSString stringWithFormat:@"%ld", (long)year];
    }
    if ([identifier isEqual:@"group"])
        return game.group;
    return [meta valueForKey:identifier];
}

#pragma mark Row to game

// The table and gameTableModel are updated together (see
// updateTableViews), but go through what the table last loaded anyway: a
// row index from the table must name the game the user sees in that row.
- (nullable Game *)gameForRow:(NSInteger)row {
    NSArray<Game *> *shown = self.displayedGames ?: self.gameTableModel;
    if (row < 0 || (NSUInteger)row >= shown.count)
        return nil;
    return shown[(NSUInteger)row];
}

- (nullable Game *)gameForView:(NSView *)view {
    return [self gameForRow:[self.gameTableView rowForView:view]];
}

- (NSIndexSet *)targetRowIndexes {
    NSIndexSet *rows = self.gameTableView.selectedRowIndexes;
    NSInteger clicked = self.gameTableView.clickedRow;
    // A right click outside the selection acts on the clicked row alone
    if (clicked != -1 && ![rows containsIndex:(NSUInteger)clicked])
        rows = [NSIndexSet indexSetWithIndex:(NSUInteger)clicked];
    return rows;
}

- (NSArray<Game *> *)targetGames {
    NSArray<Game *> *shown = self.displayedGames ?: self.gameTableModel;
    NSMutableArray<Game *> *games = [NSMutableArray new];
    [self.targetRowIndexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        if (idx < shown.count)
            [games addObject:shown[idx]];
    }];
    return games;
}

#pragma mark Editing

static NSLocale *englishUSLocale(void) {
    static NSLocale *locale;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    });
    return locale;
}

//! Matches a typed language name ("swedish") or code ("sv", "swe") to a
//! language code, and sets *languageAsWord to its English name. A name
//! that cannot be matched is kept as both code and name.
static NSString *languageCodeForTypedLanguage(NSString *typed, NSString * __autoreleasing *languageAsWord) {
    static NSDictionary<NSString *, NSString *> *codesByName;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSArray<NSString *> *codes = [NSLocale ISOLanguageCodes];
        NSMutableDictionary<NSString *, NSString *> *dict = [NSMutableDictionary dictionaryWithCapacity:codes.count];
        for (NSString *code in codes) {
            NSString *name = [englishUSLocale() displayNameForKey:NSLocaleLanguageCode value:code];
            if (name)
                dict[name.lowercaseString] = code;
        }
        codesByName = dict;
    });

    NSString *code = codesByName[typed.lowercaseString];
    if (code) {
        *languageAsWord = typed;
        return code;
    }
    code = (typed.length > 3 ? [typed substringToIndex:2] : typed).lowercaseString;
    NSString *name = [englishUSLocale() displayNameForKey:NSLocaleLanguageCode value:code];
    if (!name) {
        *languageAsWord = typed;
        return typed;
    }
    *languageAsWord = name;
    return code;
}

//! The forgiveness rating names, indexed by Metadata.forgivenessNumeric
static NSString *forgivenessName(NSInteger value) {
    NSArray<NSString *> *names = @[ @"", @"Cruel", @"Nasty", @"Tough", @"Polite", @"Merciful" ];
    if (value <= 0 || (NSUInteger)value >= names.count)
        return nil;
    return names[(NSUInteger)value];
}

- (IBAction)endedEditing:(id)sender {
    NSTextField *textField = (NSTextField *)sender;
    if (textField) {
        self.canEdit = NO;
        Game *game = [self gameForView:sender];
        if (!game)
            return;
        NSInteger col = [self.gameTableView columnForView:sender];
        if ((NSUInteger)col >= self.gameTableView.tableColumns.count)
            return;

        Metadata *meta = game.metadata;
        NSString *key = self.gameTableView.tableColumns[(NSUInteger)col].identifier;
        BOOL isGroup = [key isEqualToString:@"group"];
        NSString *oldval = isGroup ? game.group : [meta valueForKey:key];

        // Text the user didn't change is not an edit, even if the stored
        // value has changed under the open cell (Download Info)
        NSTableCellView *cellView = (NSTableCellView *)textField.superview;
        NSString *shownValue = [cellView isKindOfClass:[NSTableCellView class]] ? cellView.objectValue : nil;
        if ([shownValue isKindOfClass:[NSString class]] && [textField.stringValue isEqualToString:shownValue]) {
            NSInteger row = [self.gameTableView rowForView:sender];
            if (row >= 0)
                [self.gameTableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row] columnIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)col]];
            return;
        }

        NSString *value = [textField.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (value.length == 0)
            value = nil;
        if ([value isEqual: oldval] || (value == oldval))
            return;

        // The group column is backed by Game.group, not Metadata. Store it there
        // and move the game's folder to match, as with a title change.
        if (isGroup) {
            game.group = value;
            [[LibraryOrganizer sharedOrganizer] reorganiseGame:game];
            return;
        }

        if ([key isEqualToString:@"title"]) {
            if (value.length == 0) {
                textField.stringValue = meta.title;
                return;
            }
        }

        meta.userEdited = @(YES);
        meta.source = @(kUser);
        game.metadata.lastModified = [NSDate date];

        if ([key isEqualToString:@"firstpublishedDate"]) {
            // Clearing the cell clears the year
            NSDate *date = nil;
            if (value.length) {
                NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
                dateFormatter.dateFormat = @"yyyy";
                date = [dateFormatter dateFromString:value];
            }
            meta.firstpublishedDate = date;
            meta.firstpublished = value;
            return;
        }

        [meta setValue: value forKey: key];

        if ([key isEqualToString:@"languageAsWord"] && value.length > 1) {
            NSString *languageAsWord = value;
            meta.language = languageCodeForTypedLanguage(value, &languageAsWord);
            meta.languageAsWord = languageAsWord.capitalizedString;
        }
    }
}


- (IBAction)editedStarValue:(id)sender {
    NSLevelIndicator *indicator = (NSLevelIndicator *)sender;
    NSNumber *value = indicator.objectValue;
    NSInteger integerValue;
    if (value == nil)
        integerValue = 0;
    else
        integerValue = value.integerValue;
    Game *game = [self gameForView:sender];
    if (!game)
        return;
    if (integerValue != game.metadata.myRating.integerValue && !(integerValue == 0 && game.metadata.myRating == nil)) {
        if (integerValue == 0)
            game.metadata.myRating = nil;
        else
            game.metadata.myRating = [NSString stringWithFormat:@"%@", value];
        game.metadata.lastModified = [NSDate date];
    }
}

- (IBAction)changedForgivenessValue:(id)sender {
    NSPopUpButton *popUp = (NSPopUpButton *)sender;
    NSInteger selInteger = popUp.selectedItem.tag;
    if (@available(macOS 10.14, *)) {
        if (selInteger == 0) {
            popUp.title = @"";
        }
    } else {
        if (selInteger == 0) {
            popUp.menu.itemArray[0].title = @"  ";
            popUp.title = @"  ";
        }
    }

    Game *game = [self gameForView:sender];
    if (!game)
        return;
    if (selInteger != game.metadata.forgivenessNumeric.integerValue) {
        if (selInteger == 0) {
            if (game.metadata.forgivenessNumeric == nil)
                return;
            game.metadata.forgivenessNumeric = nil;
            game.metadata.forgiveness = nil;
        } else {
            game.metadata.forgivenessNumeric = @(selInteger);
            game.metadata.forgiveness = forgivenessName(selInteger);
        }
    }
}

- (IBAction)pushedContextualButton:(id)sender {
    NSInteger row = [self.gameTableView rowForView:sender];
    if (![self gameForRow:row])
        return;
    if (![self.gameTableView.selectedRowIndexes containsIndex:(NSUInteger)row])
        [self.gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row] byExtendingSelection:NO];
    [NSMenu popUpContextMenu:self.gameTableView.menu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)liked:(id)sender {
    Game *game = [self gameForView:sender];
    if (game)
        [self toggleLike:1 forGames:@[game]];
}

#pragma mark Mouse over

//! Shows or hides the empty-rating placeholder and the like heart of a
//! row, for a game that has neither.
- (void)setHovered:(BOOL)hovered row:(NSInteger)row likeColumn:(NSInteger)likeColumnIdx myRatingColumn:(NSInteger)myRatingColumnIdx {
    Game *game = [self gameForRow:row];
    if (!game)
        return;

    if (myRatingColumnIdx > -1 && game.metadata.myRating == nil) {
        RatingsCellView *rating = [self.gameTableView viewAtColumn:myRatingColumnIdx row:row makeIfNecessary:NO];
        rating.rating.placeholderVisibility = hovered ? NSLevelIndicatorPlaceholderVisibilityAlways : NSLevelIndicatorPlaceholderVisibilityWhileEditing;
        rating.needsDisplay = YES;
    }

    if (likeColumnIdx > -1 && game.like == 0) {
        LikeCellView *like = [self.gameTableView viewAtColumn:likeColumnIdx row:row makeIfNecessary:NO];
        like.likeButton.image = hoverLikeImage(hovered);
        like.needsDisplay = YES;
    }
}

- (void)mouseOverChangedFromRow:(NSInteger)lastRow toRow:(NSInteger)currentRow {
    NSInteger likeColumnIdx = [self.gameTableView columnWithIdentifier:@"like"];
    NSInteger myRatingColumnIdx = [self.gameTableView columnWithIdentifier:@"myRating"];

    [self setHovered:NO row:lastRow likeColumn:likeColumnIdx myRatingColumn:myRatingColumnIdx];
    [self setHovered:YES row:currentRow likeColumn:likeColumnIdx myRatingColumn:myRatingColumnIdx];
}

#pragma mark Column sizing

- (CGFloat)tableView:(NSTableView *)tableView sizeToFitWidthOfColumn:(NSInteger)columnIndex {
    CGFloat longestWidth = 0.0;
    if (tableView == self.gameTableView) {
        NSTableColumn *column = tableView.tableColumns[(NSUInteger)columnIndex];
        NSString *identifier = column.identifier;

        if ([identifier isEqual:@"found"] || [identifier isEqual:@"like"]) {
            return column.maxWidth;
        }

        BOOL isTitle = [identifier isEqual:@"title"];

        BOOL isSortColumn = [self.gameSortColumn isEqual:column.identifier];

        NSSize headerSize;
        headerSize = [column.headerCell.stringValue sizeWithAttributes:[column.headerCell.attributedStringValue attributesAtIndex:0 effectiveRange:nil]];

        longestWidth = headerSize.width + 9;

        // Add width of sort direction indicator
        if (isSortColumn) {
            longestWidth += 24;
        }

        if (@available(macOS 12.0, *)) {
        } else {
            if ([identifier isEqual:@"seriesnumber"] && longestWidth < 111) {
                return 111;
            }
        }

        if ([identifier isEqual:@"starRating"] || [identifier isEqual:@"myRating"] || [identifier isEqual:@"forgivenessNumeric"]) {
            return longestWidth;
        }

        for (Game *game in self.gameTableModel) {
            NSString *string = [self stringForGame:game columnIdentifier:identifier];
            if (string.length) {
                CGFloat viewWidth =  [string sizeWithAttributes:@{NSFontAttributeName: [NSFont systemFontOfSize:12]}].width + 5;
                // Add width of context menu button
                if (isTitle)
                    viewWidth += 25;
                if (viewWidth > longestWidth) {
                    longestWidth = viewWidth;
                    if (longestWidth >= column.maxWidth)
                        return column.maxWidth;
                }
            }
        }
    }

    return longestWidth;
}

#pragma mark Selection

- (void)tableViewSelectionDidChange:(id)notification {
    NSTableView *tableView = [notification object];
    if (tableView == self.gameTableView) {
        NSIndexSet *rows = tableView.selectedRowIndexes;
        if (self.gameTableModel.count && rows.count) {
            self.selectedGames = [self.gameTableModel objectsAtIndexes:rows];
            Game *game = self.selectedGames[0];
            if (!game.theme) {
                game.theme = [Preferences currentTheme];
            } else {
                Preferences *prefs = Preferences.instance;
                if (prefs && prefs.currentGame == nil && !prefs.lightOverrideActive && !prefs.darkOverrideActive && self.view.window.keyWindow) {
                    [prefs restoreThemeSelection:game.theme];
                }
            }
        } else self.selectedGames = @[];

        [self postSideViewUpdate];
        [self invalidateRestorableState];
    }
}

- (NSString *)tableView:(NSTableView *)tableView typeSelectStringForTableColumn:(NSTableColumn *)tableColumn
                    row:(NSInteger)row {
    if ([tableColumn.identifier isEqualToString:@"title"]) {
        return self.gameTableModel[(NSUInteger)row].metadata.title;
    }
    return nil;
}

#pragma mark Core Data changes

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    self.gameTableDirty = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self updateTableViews];
    });
    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];

    for (id obj in updatedObjects) {
        if ([obj isKindOfClass:[Theme class]]) {
            [self rebuildThemesSubmenu];
            break;
        }
    }

    [self updateWindowSubtitle];
}

#pragma mark -
#pragma mark Open game on double click, edit on click and wait

- (void)doClick:(id)sender {
    // Click and wait to rename, the way the Finder does it: the rename is
    // armed only by a plain single click on the one row that was already
    // selected, by itself, before the mouse went down. The click that
    // selects a row, a click with a modifier key held, and the first click
    // of a double click all leave the table alone.
    if (!self.canEdit)
        return;
    LibHelperTableView *tableView = self.gameTableView;
    NSInteger row = tableView.clickedRow;
    NSInteger column = tableView.clickedColumn;
    if (row == -1 || column == -1)
        return;

    NSEvent *event = NSApp.currentEvent;
    if (event.type != NSEventTypeLeftMouseUp && event.type != NSEventTypeLeftMouseDown)
        return;
    if (event.clickCount != 1)
        return;
    NSEventModifierFlags held = event.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagShift | NSEventModifierFlagOption | NSEventModifierFlagControl);
    if (held)
        return;

    NSIndexSet *before = tableView.selectionBeforeClick;
    if (before.count != 1 || ![before containsIndex:(NSUInteger)row])
        return;
    NSIndexSet *now = tableView.selectedRowIndexes;
    if (now.count != 1 || ![now containsIndex:(NSUInteger)row])
        return;

    // The rename opens only if nothing has cancelled it by the time the
    // delay is up (doDoubleClick bumps the token) and the same row is
    // still the only one selected.
    NSUInteger token = ++self.pendingRenameToken;
    TableViewController * __weak weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        TableViewController *strongSelf = weakSelf;
        if (!strongSelf || !strongSelf.canEdit || strongSelf.pendingRenameToken != token)
            return;
        NSIndexSet *selected = strongSelf.gameTableView.selectedRowIndexes;
        if (selected.count != 1 || ![selected containsIndex:(NSUInteger)row])
            return;
        if (row >= strongSelf.gameTableView.numberOfRows || column >= strongSelf.gameTableView.numberOfColumns)
            return;
        [strongSelf.gameTableView editColumn:column row:row withEvent:nil select:YES];
    });
}

// DoubleAction
- (void)doDoubleClick:(id)sender {
    if (self.gameTableView.clickedRow == -1) {
        return;
    }
    // Drop the rename the first click scheduled: a double click plays the
    // game, and a title cell left open under a starting game is how a
    // rename ends up on the wrong row.
    self.pendingRenameToken++;
    [self enableClickToRenameAfterDelay];
    [self play:sender];
}

- (void)enableClickToRenameAfterDelay {
    self.canEdit = NO;
    TableViewController * __weak weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
        TableViewController *strongSelf = weakSelf;
        if (strongSelf) {
            strongSelf.canEdit = YES;
        }
    });
}

@end

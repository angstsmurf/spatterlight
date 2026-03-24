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

enum  {
    FORGIVENESS_NONE,
    FORGIVENESS_CRUEL,
    FORGIVENESS_NASTY,
    FORGIVENESS_TOUGH,
    FORGIVENESS_POLITE,
    FORGIVENESS_MERCIFUL
};

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
    NSTableColumn *column;
    for (NSTableColumn *tableColumn in self.gameTableView.tableColumns) {
        if ([tableColumn.identifier
             isEqualToString:[item valueForKey:@"identifier"]]) {
            column = tableColumn;
            break;
        }
    }
    if (item.state == YES) {
        [column setHidden:YES];
        item.state = NO;
    } else {
        [column setHidden:NO];
        item.state = YES;
    }
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

+ (NSInteger)stringcompare:(NSString *)a with:(NSString *)b {
    if ([a hasPrefix: @"The "] || [a hasPrefix: @"the "])
        a = [a substringFromIndex: 4];
    if ([b hasPrefix: @"The "] || [b hasPrefix: @"the "])
        b = [b substringFromIndex: 4];
    return [a localizedStandardCompare: b];
}

+ (NSInteger)compareGame:(Metadata *)a with:(Metadata *)b key:(id)key ascending:(BOOL)ascending {
    NSString * ael = [a valueForKey:key];
    NSString * bel = [b valueForKey:key];
    return [TableViewController compareString:ael withString:bel ascending:ascending];
}

+ (NSInteger)compareString:(NSString *)ael withString:(NSString *)bel ascending:(BOOL)ascending {
    if ((!ael || ael.length == 0) && (!bel || bel.length == 0))
        return NSOrderedSame;
    if (!ael || ael.length == 0) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!bel || bel.length == 0) return ascending ? NSOrderedAscending : NSOrderedDescending;

    return [TableViewController stringcompare:ael with:bel];
}

+ (NSInteger)compareDate:(NSDate *)ael withDate:(NSDate *)bel ascending:(BOOL)ascending {
    if ((!ael) && (!bel))
        return NSOrderedSame;
    if (!ael) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (!bel) return ascending ? NSOrderedAscending : NSOrderedDescending;
    return [ael compare:bel];
}

+ (NSInteger)compareIntegers:(NSInteger)a and:(NSInteger)b ascending:(BOOL)ascending {
    if (a == b)
        return NSOrderedSame;
    if (a == NSNotFound) return ascending ? NSOrderedDescending : NSOrderedAscending;
    if (b == NSNotFound) return ascending ? NSOrderedAscending : NSOrderedDescending;
    if (a < b)
        return NSOrderedAscending;
    else
        return NSOrderedDescending;
}

+ (NSPredicate *)searchPredicateForWord:(NSString *)word {
    return [NSPredicate predicateWithFormat: @"(detectedFormat contains [c] %@) OR (metadata.title contains [c] %@) OR (metadata.author contains [c] %@) OR (metadata.group contains [c] %@) OR (metadata.genre contains [c] %@) OR (metadata.series contains [c] %@) OR (metadata.seriesnumber contains [c] %@) OR (metadata.forgiveness contains [c] %@) OR (metadata.languageAsWord contains [c] %@) OR (metadata.firstpublished contains %@)", word, word, word, word, word, word, word, word, word, word, word];
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

    NSCompoundPredicate *comp;

    BOOL alreadySearched = NO;

    if (self.searchString.length)
    {
        // First we search using the entire search string as a phrase
        [predicateArray addObject:[TableViewController searchPredicateForWord:self.searchString]];

        comp = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];

        fetchRequest.predicate = comp;
        error = nil;
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
                        [predicateArray addObject:
                         [TableViewController searchPredicateForWord:word]];
                }
            }
        }
    }

    if (!searchResult.count && !alreadySearched) {
        comp = [NSCompoundPredicate andPredicateWithSubpredicates: predicateArray];
        fetchRequest.predicate = comp;
        error = nil;
        searchResult = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];
    }

    self.gameTableModel = searchResult.mutableCopy;
    if (self.gameTableModel == nil)
        NSLog(@"Problem! %@",error);

    TableViewController * __weak weakSelf = self;

    NSSortDescriptor *sort = [NSSortDescriptor sortDescriptorWithKey:@"self" ascending:self.sortAscending comparator:^(Game *aid, Game *bid) {

        NSInteger cmp = 0;

        TableViewController *strongSelf = weakSelf;

        if (!strongSelf)
            return cmp;

        Metadata *a = aid.metadata;
        Metadata *b = bid.metadata;
        if ([strongSelf.gameSortColumn isEqual:@"firstpublishedDate"]) {
            cmp = [TableViewController compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"added"] || [strongSelf.gameSortColumn isEqual:@"lastPlayed"]) {
            cmp = [TableViewController compareDate:[aid valueForKey:strongSelf.gameSortColumn] withDate:[bid valueForKey:strongSelf.gameSortColumn] ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"lastModified"]) {
            cmp = [TableViewController compareDate:[a valueForKey:strongSelf.gameSortColumn] withDate:[b valueForKey:strongSelf.gameSortColumn] ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"found"]) {
            NSString *string1, *string2;
            [strongSelf imageNameForGame:aid description:&string1];
            [strongSelf imageNameForGame:bid description:&string2];
            cmp = [TableViewController compareString:string1 withString:string2 ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"like"]) {
            NSInteger numA = aid.like;
            NSInteger numB = bid.like;
            if (numA == 1) {
                numA = 2;
            } else if (numA == 2) {
                numA = 1;
            }
            if (numB == 1) {
                numB = 2;
            } else if (numA == 2) {
                numB = 1;
            }
            cmp = [TableViewController compareIntegers:numA and:numB ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"forgivenessNumeric"]) {
            NSString *string1 = a.forgivenessNumeric ? [NSString stringWithFormat:@"%@", a.forgivenessNumeric] : nil;
            NSString *string2 = b.forgivenessNumeric ? [NSString stringWithFormat:@"%@", b.forgivenessNumeric] : nil;
            cmp = [TableViewController compareString:string1 withString:string2 ascending:strongSelf.sortAscending];

            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"seriesnumber"]) {
            NSInteger numA = a.seriesnumber ? a.seriesnumber.integerValue : NSNotFound;
            NSInteger numB = b.seriesnumber ? b.seriesnumber.integerValue : NSNotFound;
            cmp = [TableViewController compareIntegers:numA and:numB ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"series"]) {
            cmp = [TableViewController compareGame:a with:b key:strongSelf.gameSortColumn ascending:strongSelf.sortAscending];
            if (cmp == NSOrderedSame) {
                NSInteger numA = a.seriesnumber ? a.seriesnumber.integerValue : NSNotFound;
                NSInteger numB = b.seriesnumber ? b.seriesnumber.integerValue : NSNotFound;
                cmp = [TableViewController compareIntegers:numA and:numB ascending:strongSelf.sortAscending];
                if (cmp) return cmp;
            } else return cmp;
        } else if ([strongSelf.gameSortColumn isEqual:@"format"]) {
            cmp = [TableViewController compareString:aid.detectedFormat withString:bid.detectedFormat ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        } else if (strongSelf.gameSortColumn) {
            cmp = [TableViewController compareGame:a with:b key:strongSelf.gameSortColumn ascending:strongSelf.sortAscending];
            if (cmp) return cmp;
        }
        cmp = [TableViewController compareGame:a with:b key:@"title" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [TableViewController compareGame:a with:b key:@"author" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [TableViewController compareGame:a with:b key:@"seriesnumber" ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        cmp = [TableViewController compareDate:a.firstpublishedDate withDate:b.firstpublishedDate ascending:strongSelf.sortAscending];
        if (cmp) return cmp;
        return (NSInteger)[aid.hashTag compare:bid.hashTag];
    }];

    NSSortDescriptor *fallback = [NSSortDescriptor sortDescriptorWithKey:@"hashTag" ascending:self.sortAscending comparator:^(NSString *a, NSString *b) {
        return [a compare:b];
    }];

    [self.gameTableModel sortUsingDescriptors:@[sort, fallback]];

    dispatch_async(dispatch_get_main_queue(), ^{
        weakSelf.gameTableDirty = NO;
        [weakSelf.gameTableView reloadData];
        [weakSelf selectGames:[NSSet setWithArray:weakSelf.selectedGames]];
        [weakSelf invalidateRestorableState];
    });
}

#pragma mark NSTableViewDelegate / DataSource

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

- (NSString *)imageNameForGame:(Game *)game description:(NSString * __autoreleasing *)description {
    NSString *name;
    if (!game.found) {
        *description = NSLocalizedString(@"File not found", nil);
        name = @"exclamationmark.circle";
    } else {
        BOOL playing = NO;
        if (game.hashTag.length == 0) {
            game.hashTag = [game.path signatureFromFile];
            if (game.hashTag.length == 0)
                return nil;
            game.metadata.hashTag = game.hashTag;
        }
        GlkController *session = self.gameSessions[game.hashTag];
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

- (nullable NSView *)tableView:(NSTableView*)tableView viewForTableColumn:(nullable NSTableColumn *)column row:(NSInteger)row {

    NSTableCellView *cellView = [tableView makeViewWithIdentifier:column.identifier owner:self];

    if (tableView == self.gameTableView) {
        NSString *string = nil;
        if ((NSUInteger)row >= self.gameTableModel.count) return nil;

        NSMutableAttributedString *headerAttrStr = column.headerCell.attributedStringValue.mutableCopy;

        if (headerAttrStr.length) {
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

        Game *game = self.gameTableModel[(NSUInteger)row];
        Metadata *meta = game.metadata;

        NSString *identifier = column.identifier;

        if ([identifier isEqual:@"found"]) {
            NSString *description;
            NSString *name = [self imageNameForGame:game description:&description];
            if (@available(macOS 11.0, *)) {
                cellView.imageView.image = [NSImage imageWithSystemSymbolName:name accessibilityDescription:description];
            } else {
                cellView.imageView.image = [NSImage imageNamed:name];
                cellView.imageView.image.accessibilityDescription = description;
            }
            cellView.imageView.accessibilityLabel = description;
        } else if ([identifier isEqual:@"like"]) {
            LikeCellView *likeCellView = (LikeCellView *)cellView;
            switch (game.like) {
                case 2:
                    if (@available(macOS 11.0, *)) {
                        likeCellView.likeButton.image = [NSImage imageWithSystemSymbolName:@"heart.slash.fill" accessibilityDescription:NSLocalizedString(@"Disliked", nil)];
                    } else {
                        likeCellView.likeButton.image = [NSImage imageNamed:@"heart.slash.fill"];
                        likeCellView.likeButton.image.accessibilityDescription = NSLocalizedString(@"Disliked", nil);
                    }
                    likeCellView.toolTip = NSLocalizedString(@"Disliked", nil);
                    likeCellView.accessibilityLabel = NSLocalizedString(@"Disliked", nil);
                    break;
                case 1:
                    if (@available(macOS 11.0, *)) {
                        likeCellView.likeButton.image = [NSImage imageWithSystemSymbolName:@"heart.fill" accessibilityDescription:NSLocalizedString(@"Liked", nil)];
                    } else {
                        likeCellView.likeButton.image = [NSImage imageNamed:@"heart.fill"];
                        likeCellView.likeButton.image.accessibilityDescription = NSLocalizedString(@"Liked", nil);
                    }
                    likeCellView.toolTip = NSLocalizedString(@"Liked", nil);
                    likeCellView.accessibilityLabel = NSLocalizedString(@"Liked", nil);
                    break;
                default:
                    if (row == self.gameTableView.mouseOverRow) {
                        if (@available(macOS 11.0, *)) {
                            likeCellView.likeButton.image = [NSImage imageWithSystemSymbolName:@"heart" accessibilityDescription:NSLocalizedString(@"Like", nil)];
                        } else {
                            likeCellView.likeButton.image =  [NSImage imageNamed:@"heart"];
                            likeCellView.likeButton.image.accessibilityDescription = NSLocalizedString(@"Like", nil);
                        }
                    } else {
                        likeCellView.likeButton.image = nil;
                    }
                    likeCellView.toolTip = NSLocalizedString(@"Like", nil);
                    likeCellView.accessibilityLabel = NSLocalizedString(@"Not liked", nil);
                    break;
            }
            return likeCellView;
        } else if ([identifier isEqual:@"format"]) {
            if (!game.detectedFormat)
                game.detectedFormat = meta.format;
            string = game.detectedFormat;
        } else if ([identifier isEqual:@"added"] || [identifier isEqual:@"lastPlayed"]) {
            string = [[game valueForKey: identifier] formattedRelativeString];
        } else if ([identifier isEqual:@"lastModified"]) {
            string = [meta.lastModified formattedRelativeString];
        } else if ([identifier isEqual:@"firstpublishedDate"]) {
            NSDate *date = meta.firstpublishedDate;
            if (date) {
                NSDateComponents *components = [[NSCalendar currentCalendar] components:NSCalendarUnitDay | NSCalendarUnitMonth | NSCalendarUnitYear fromDate:date];
                string = [NSString stringWithFormat:@"%ld", components.year];
            }
        } else if ([identifier isEqual:@"forgivenessNumeric"]) {
            ForgivenessCellView *forgivenessCell = (ForgivenessCellView *)cellView;
            NSInteger value = meta.forgivenessNumeric.integerValue;
            if (value < 0 || value > 5)
                value = 0;
            NSPopUpButton *popUp = forgivenessCell.popUpButton;
            NSMenuItem *none = popUp.menu.itemArray[0];
            if (value == 0) {
                [popUp selectItem:none];
                none.state = NSOnState;
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
            return forgivenessCell;
        } else if ([identifier isEqual:@"starRating"] ||
                   [identifier isEqual:@"myRating"]) {
            NSInteger value;
            BOOL isMy = [identifier isEqual:@"myRating"];

            RatingsCellView *indicatorCell = (RatingsCellView *)cellView;
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
            return indicatorCell;
        } else {
            string = [meta valueForKey: identifier];
        }
        if (string == nil)
            string = @"";

        cellView.textField.stringValue = string;
    }
    return cellView;
}

#pragma mark Editing

- (IBAction)endedEditing:(id)sender {
    NSTextField *textField = (NSTextField *)sender;
    if (textField) {
        self.canEdit = NO;
        NSInteger row = [self.gameTableView rowForView:sender];
        if ((NSUInteger)row >= self.gameTableModel.count)
            return;
        NSInteger col = [self.gameTableView columnForView:sender];
        if ((NSUInteger)col >= self.gameTableView.tableColumns.count)
            return;

        Game *game = self.gameTableModel[(NSUInteger)row];
        Metadata *meta = game.metadata;
        NSString *key = self.gameTableView.tableColumns[(NSUInteger)col].identifier;
        NSString *oldval = [meta valueForKey:key];

        NSString *value = [textField.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (value.length == 0)
            value = nil;
        if ([value isEqual: oldval] || (value == oldval))
            return;

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
            NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
            dateFormatter.dateFormat = @"yyyy";
            meta.firstpublishedDate = [dateFormatter dateFromString:value];
            meta.firstpublished = value;
            return;
        }

        [meta setValue: value forKey: key];

        if ([key isEqualToString:@"languageAsWord"]) {
            NSString *languageAsWord = value;
            NSString *languageCode;
            if (languageAsWord.length > 1) {
                languageCode = self.languageCodes[languageAsWord.lowercaseString];
                if (!languageCode) {
                    languageCode = value;
                    if (languageCode.length > 3) {
                        languageCode = [languageAsWord substringToIndex:2];
                    }
                    languageCode = languageCode.lowercaseString;
                    languageAsWord = [self.englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
                    if (!languageAsWord) {
                        languageCode = value;
                        languageAsWord = value;
                    }
                }
                meta.language = languageCode;
                meta.languageAsWord = languageAsWord.capitalizedString;
            }
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
    NSInteger row = [self.gameTableView rowForView:sender];
    if ((NSUInteger)row >= self.gameTableModel.count)
        return;
    Game *game = self.gameTableModel[(NSUInteger)row];
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

    NSInteger row = [self.gameTableView rowForView:sender];
    if ((NSUInteger)row >= self.gameTableModel.count)
        return;
    Game *game = self.gameTableModel[(NSUInteger)row];
    if (selInteger != game.metadata.forgivenessNumeric.integerValue) {
        if (selInteger == 0) {
            if (game.metadata.forgivenessNumeric == nil)
                return;
            game.metadata.forgivenessNumeric = nil;
            game.metadata.forgiveness = nil;
        } else {
            game.metadata.forgivenessNumeric = @(selInteger);
            game.metadata.forgiveness = self.forgiveness[game.metadata.forgivenessNumeric];
        }
    }
}

- (IBAction)pushedContextualButton:(id)sender {
    NSInteger row = [self.gameTableView rowForView:sender];
    if ((NSUInteger)row >= self.gameTableModel.count)
        return;
    if (![self.gameTableView.selectedRowIndexes containsIndex:(NSUInteger)row])
        [self.gameTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row] byExtendingSelection:NO];
    [NSMenu popUpContextMenu:self.gameTableView.menu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)liked:(id)sender {
    NSInteger row = [self.gameTableView rowForView:sender];
    if ((NSUInteger)row >= self.gameTableModel.count)
        return;
    Game *game = self.gameTableModel[(NSUInteger)row];
    if (game.like == 1) {
        game.like = 0;
    } else {
        game.like = 1;
    }
}

#pragma mark Mouse over

- (void)mouseOverChangedFromRow:(NSInteger)lastRow toRow:(NSInteger)currentRow {
    NSInteger likeColumnIdx = -1;
    NSInteger myRatingColumnIdx = -1;

    NSInteger index = 0;

    for (NSTableColumn *column in self.gameTableView.tableColumns) {
        if (myRatingColumnIdx == -1 && [column.identifier isEqual:@"myRating"]) {
            myRatingColumnIdx = index;
        } else if (likeColumnIdx == -1 && [column.identifier isEqual:@"like"]) {
            likeColumnIdx = index;
        }

        index++;
    }

    RatingsCellView *rating;
    Game *game;

    if (lastRow > -1 && lastRow < (NSInteger)self.gameTableModel.count) {
        game = self.gameTableModel[(NSUInteger)lastRow];
        if (myRatingColumnIdx > -1 && game.metadata.myRating == 0) {
            rating = [self.gameTableView viewAtColumn:myRatingColumnIdx row:lastRow makeIfNecessary:NO];
            rating.rating.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityWhileEditing;
            rating.needsDisplay = YES;
        }

        if (likeColumnIdx > -1 && game.like == 0) {
            LikeCellView *like = [self.gameTableView viewAtColumn:likeColumnIdx row:lastRow makeIfNecessary:NO];
            like.likeButton.image = nil;
            like.needsDisplay = YES;
        }
    }

    if (currentRow > -1 && currentRow < (NSInteger)self.gameTableModel.count) {
        game = self.gameTableModel[(NSUInteger)currentRow];

        if (myRatingColumnIdx > -1 && game.metadata.myRating == 0) {
            rating = [self.gameTableView viewAtColumn:myRatingColumnIdx row:currentRow makeIfNecessary:NO];
            rating.rating.placeholderVisibility = NSLevelIndicatorPlaceholderVisibilityAlways;
            rating.needsDisplay = YES;
        }


        if (likeColumnIdx > -1 && game.like == 0) {
            LikeCellView *like = [self.gameTableView viewAtColumn:likeColumnIdx row:currentRow makeIfNecessary:NO];
            like.likeButton.image = [NSImage imageNamed:@"heart"];
            like.needsDisplay = YES;
        }
    }
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
        BOOL isFormat = [identifier isEqual:@"format"];
        BOOL isDate = ([identifier isEqual:@"added"] || [identifier isEqual:@"lastPlayed"]);
        BOOL isYear = [identifier isEqual:@"firstpublishedDate"];
        BOOL isModified = [identifier isEqual:@"lastModified"];

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
            NSString *string;
            if (isFormat) {
                string = game.detectedFormat;
            } else if (isDate) {
                string = [[game valueForKey: identifier] formattedRelativeString];
            } else if (isModified) {
                string = [game.metadata.lastModified formattedRelativeString];
            } else if (isYear) {
                string = game.metadata.firstpublished;
            } else {
                string = [game.metadata valueForKey:identifier];
            }
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

        NSArray<Game *> *selected = self.selectedGames;
        if (selected.count > 2)
            selected = @[self.selectedGames[0], self.selectedGames[1]];
        
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"UpdateSideView" object:selected]];
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
    NSSet *insertedObjects = (notification.userInfo)[NSInsertedObjectsKey];

    for (id obj in updatedObjects) {
        if (self.countingMetadataChanges && [obj isKindOfClass:[Metadata class]]) {
            self.updatedMetadataCount++;
        }

        if ([obj isKindOfClass:[Theme class]]) {
            [self rebuildThemesSubmenu];
            break;
        }
    }

    for (id obj in insertedObjects) {
        if (self.countingMetadataChanges &&
            [obj isKindOfClass:[Metadata class]]) {
            self.insertedMetadataCount++;
        }
    }

    if (@available(macOS 11.0, *)) {
        NSArray *games = [Fetches fetchObjects:@"Game" predicate:@"hidden == NO" inContext:self.managedObjectContext];
        if (games.count == 0)
            self.view.window.subtitle = @"No games";
        else if (games.count == 1)
            self.view.window.subtitle = @"1 game";
        else
            self.view.window.subtitle = [NSString stringWithFormat:@"%ld games", games.count];
    }
}

#pragma mark -
#pragma mark Open game on double click, edit on click and wait

- (void)doClick:(id)sender {
    if (self.canEdit) {
        NSInteger row = self.gameTableView.clickedRow;
        if ([self.gameTableView.selectedRowIndexes containsIndex:(NSUInteger)row]) {
            TableViewController * __weak weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void) {
                TableViewController *strongSelf = weakSelf;
                if (strongSelf && strongSelf.canEdit) {
                    NSInteger selectedRow = strongSelf.gameTableView.selectedRow;
                    NSInteger column = strongSelf.gameTableView.selectedColumn;
                    if (selectedRow != -1 && column != -1) {
                        [strongSelf.gameTableView editColumn:column row:selectedRow withEvent:nil select:YES];
                    }
                }
            });
        }
    }
}

// DoubleAction
- (void)doDoubleClick:(id)sender {
    if (self.gameTableView.clickedRow == -1) {
        return;
    }
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

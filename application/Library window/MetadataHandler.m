//
//  MetadataHandler.m
//  Spatterlight
//
//  Extracted from TableViewController.m
//

#import "MetadataHandler.h"

#import "TableViewController.h"
#import "TableViewController+Internal.h"
#import "TableViewController+TableDelegate.h"

#import "LibHelperTableView.h"
#import "LibController.h"
#import "GameLauncher.h"
#import "AppDelegate.h"
#import "Preferences.h"
#import "Constants.h"

#import "NSString+Categories.h"
#import "NSData+Categories.h"
#import "NSManagedObjectContext+safeSave.h"

#import "Game.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Image.h"
#import "Fetches.h"

#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFIdentification.h"
#import "IFDBDownloader.h"

#import "GameImporter.h"
#import "NotificationBezel.h"
#import "CoreDataManager.h"

enum { X_EDITED, X_LIBRARY, X_DATABASE }; // export selections

typedef NS_ENUM(int32_t, kImportResult) {
    kImportResultNoneFound,
    kImportResultExactMatchesOnly,
    kImportResultPartialMatchesOnly,
    kImportExactAndPartialMatchesBoth
};

@implementation MetadataHandler

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController {
    self = [super init];
    if (self) {
        _tableViewController = tableViewController;
    }
    return self;
}

#pragma mark - Import / Export UI

- (void)importMetadataInContext:(NSManagedObjectContext *)ctx window:(NSWindow *)window {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.prompt = NSLocalizedString(@"Import", nil);

    panel.allowedFileTypes = @[ @"iFiction", @"ifiction" ];

    MetadataHandler * __weak weakSelf = self;

    [panel beginSheetModalForWindow:window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;
            [ctx performBlock:^{
                [weakSelf importMetadataFromFile:url.path inContext:ctx];
            }];
        }
    }];
}

- (void)exportMetadataInContext:(NSManagedObjectContext *)ctx window:(NSWindow *)window accessoryView:(NSView *)accView popUpButton:(NSPopUpButton *)exportTypeControl {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.accessoryView = accView;
    panel.allowedFileTypes = @[ @"iFiction" ];
    panel.prompt = NSLocalizedString(@"Export", nil);
    panel.nameFieldStringValue = NSLocalizedString(@"Interactive Fiction Metadata.iFiction", nil);

    [panel beginSheetModalForWindow:window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;
            [self exportMetadataToFile:url.path
                                  what:exportTypeControl
             .indexOfSelectedItem context:ctx];
        }
    }];
}

#pragma mark - Metadata XML Parsing

+ (NSDictionary<NSString *, IFStory *> *)importMetadataFromXML:(NSData *)mdbuf indirectMatches:(NSDictionary<NSString *, IFStory *> * __autoreleasing *)indirectDict inContext:(NSManagedObjectContext *)context {
    NSMutableDictionary<NSString *, IFStory *> *directMatches, *indirectMatches;
    directMatches = [[NSMutableDictionary alloc] init];

    IFictionMetadata *ifictionmetadata = [[IFictionMetadata alloc] initWithData:mdbuf];

    if (!ifictionmetadata || ifictionmetadata.stories.count == 0)
        return directMatches;

    indirectMatches = [[NSMutableDictionary alloc] init];

    for (IFStory *story in ifictionmetadata.stories) {
        for (NSString *ifid in story.identification.ifids) {
            NSArray *result = [Fetches fetchGamesForIfid:ifid inContext:context];
            if (result.count) {
                for (Game *game in result) {
                    NSString *idString = game.objectID.URIRepresentation.absoluteString;
                    directMatches[idString] = story;
                }
            } else {
                NSSet *resultSet = [Fetches fetchMetadataForIfid:ifid inContext:context];
                if (resultSet.count) {
                    for (Metadata *meta in resultSet) {
                        NSString *idString = meta.game.objectID.URIRepresentation.absoluteString;
                        if (idString == nil)
                            continue;
                        indirectMatches[idString] = story;
                    }
                }
            }
        }
    }

    if (indirectMatches.count && indirectDict != nil)
        *indirectDict = indirectMatches;

    return directMatches;
}

- (void)addInfoToMetadata:(NSSet<NSManagedObjectID *>*)gameIDs fromStories:(NSDictionary<NSString *, IFStory *> *)stories {
    TableViewController *tvc = self.tableViewController;
    [tvc.downloadQueue addOperationWithBlock:^{
        NSManagedObjectContext *childContext = tvc.coreDataManager.privateChildManagedObjectContext;
        childContext.undoManager = nil;
        childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;
        [childContext performBlock:^{
            for (NSManagedObjectID *gameID in gameIDs) {
                IFStory *story = stories[gameID.URIRepresentation.absoluteString];
                Game *game = [childContext objectWithID:gameID];
                [story addInfoToMetadata:game.metadata];
                game.metadata.source = @(kExternal);
                game.metadata.lastModified = [NSDate date];
            }
            [childContext safeSave];
        }];
    }];
}

- (void)askAboutImportingMetadata:(NSDictionary<NSString *, IFStory *> *)storyDict indirectMatches:(NSDictionary<NSString *, IFStory *> *)indirectDict {

    kImportResult __block importResult = kImportResultNoneFound;

    NSSet<Game *> __block *games = [[NSSet alloc] init];
    NSSet<Game *> __block *indirectGames = [[NSSet alloc] init];
    NSSet<NSManagedObjectID *> __block *gameIDs = [[NSSet alloc] init];
    NSSet<NSManagedObjectID *> __block *indirectGameIDs = [[NSSet alloc] init];

    NSString __block *msg = @"There were no library matches for the games in the iFiction file.";
    NSString __block *msg2;

    NSLog(@"storyDict.count: %ld indirectDict.count: %ld", storyDict.count, indirectDict.count);

    TableViewController *tvc = self.tableViewController;
    NSManagedObjectContext *childContext = tvc.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    [childContext performBlockAndWait:^{
        for (NSString *identifier in storyDict.allKeys) {
            NSManagedObjectID *objectID = [childContext.persistentStoreCoordinator managedObjectIDForURIRepresentation:[NSURL URLWithString:identifier]];
            Game *result = [childContext objectWithID:objectID];
            if (result) {
                games = [games setByAddingObject:result];
                gameIDs = [gameIDs setByAddingObject:objectID];
            }
        }

        for (NSString *identifier in indirectDict.allKeys) {
            NSManagedObjectID *objectID = [childContext.persistentStoreCoordinator managedObjectIDForURIRepresentation:[NSURL URLWithString:identifier]];
            Game *result = [childContext objectWithID:objectID];
            if (result && ![games containsObject:result]) {
                indirectGames = [indirectGames setByAddingObject:result];
                indirectGameIDs = [indirectGameIDs setByAddingObject:objectID];
            }
        }

        if (games.count > 0) {
            importResult = kImportResultExactMatchesOnly;
            msg = [NSString stringWithFormat:@"Found details matching %@ in iFiction file.",
                   [NSString stringWithSummaryOfGames:games.allObjects]];
        } else if (storyDict.count) {
            NSLog(@"askAboutImportingMetadata: None of the %ld direct matches found in database", storyDict.count);
        }

        if (indirectGames.count > 0) {
            if (importResult == kImportResultExactMatchesOnly)
                importResult = kImportExactAndPartialMatchesBoth;
            else
                importResult = kImportResultPartialMatchesOnly;

            msg2 = [NSString stringWithFormat:@"%@ound partial match%@ for %@%@.", importResult == kImportExactAndPartialMatchesBoth ? @"Also f" : @"F", indirectGames.count > 1 ? @"es" : @"", [NSString stringWithSummaryOfGames:indirectGames.allObjects], importResult == kImportExactAndPartialMatchesBoth ? @"" : @" in iFiction file"];
        } else if (indirectDict.count) {
            NSLog(@"askAboutImportingMetadata: None of the %ld indirect matches found in database (or they were also exact matches)", indirectDict.count);
        }
    }];

    CGFloat delay = 0;
    if (tvc.spinnerSpinning) {
        delay = 1;
    }

    MetadataHandler * __weak weakSelf = self;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        NSAlert *anAlert = [[NSAlert alloc] init];
        [anAlert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        anAlert.messageText = NSLocalizedString(msg, nil);

        if (importResult != kImportResultNoneFound) {
            [anAlert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
            anAlert.informativeText = NSLocalizedString(@"Would you like to replace existing information?", nil);
        }

        [anAlert beginSheetModalForWindow:tvc.view.window completionHandler:^(NSModalResponse result){
            if (importResult == kImportResultNoneFound)
                return;
            if (result == NSAlertFirstButtonReturn) {
                if (importResult == kImportResultExactMatchesOnly ||
                    importResult == kImportExactAndPartialMatchesBoth) {
                    [weakSelf addInfoToMetadata:gameIDs fromStories:storyDict];
                } else if (importResult == kImportResultPartialMatchesOnly) {
                    [weakSelf addInfoToMetadata:indirectGameIDs fromStories:indirectDict];
                }
            }
            if (importResult == kImportExactAndPartialMatchesBoth) {
                NSAlert *alert2 = [[NSAlert alloc] init];
                alert2.messageText = NSLocalizedString(msg2, nil);
                [alert2 addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
                [alert2 addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
                alert2.informativeText = anAlert.informativeText;
                [alert2 beginSheetModalForWindow:tvc.view.window completionHandler:^(NSModalResponse result2){
                    if (result2 == NSAlertFirstButtonReturn) {
                        [weakSelf addInfoToMetadata:indirectGameIDs fromStories:indirectDict];
                    }
                }];
            }
        }];
    });
}

- (void)importMetadataFromFile:(NSString *)filename inContext:(NSManagedObjectContext *)context {
    NSLog(@"libctl: importMetadataFromFile %@", filename);

    NSData *data = [NSData dataWithContentsOfFile:filename];
    if (!data)
        return;

    NSDictionary<NSString *, IFStory *> *indirectMatches = [[NSDictionary alloc] init];
    NSDictionary<NSString *, IFStory *> *exactMatches = [MetadataHandler importMetadataFromXML:data indirectMatches:&indirectMatches inContext:context];
    if (exactMatches.count > 0 || indirectMatches.count > 0) {
        [self askAboutImportingMetadata:exactMatches indirectMatches:indirectMatches];
    }
}

#pragma mark - Metadata Export

static void write_xml_text(FILE *fp, Metadata *info, NSString *key) {
    NSString *val;
    const char *tagname;
    const char *s;

    val = [info valueForKey:key];
    if (!val)
        return;

    // "description" is a reserved word in core data,
    // so we use "blurb" internally. We change this back
    // to "description" in XML output.
    if ([key isEqualToString:@"blurb"])
        key = @"description";

    NSLog(@"write_xml_text: key:%@ val:%@", key, val);

    tagname = key.UTF8String;
    s = [NSString stringWithFormat:@"%@", val].UTF8String;

    fprintf(fp, "<%s>", tagname);
    while (*s) {
        if (*s == '&')
            fprintf(fp, "&amp;");
        else if (*s == '<')
            fprintf(fp, "&lt;");
        else if (*s == '>')
            fprintf(fp, "&gt;");
        else if (*s == '\n') {
            fprintf(fp, "<br/>");
            while (s[1] == '\n')
                s++;
        } else {
            putc(*s, fp);
        }
        s++;
    }
    fprintf(fp, "</%s>\n", tagname);
}

- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what context:(NSManagedObjectContext *)context {
    NSEnumerator *enumerator;
    Metadata *meta;

    NSLog(@"libctl: exportMetadataToFile %@", filename);

    FILE *fp;

    fp = fopen(filename.UTF8String, "w");
    if (!fp)
        return NO;

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<ifindex version=\"1.0\">\n\n");

    NSFetchRequest *fetchRequest = [Metadata fetchRequest];

    switch (what) {
        case X_EDITED:
            fetchRequest.predicate = [NSPredicate predicateWithFormat: @"(userEdited == YES)"];
            break;
        case X_LIBRARY:
            fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ANY games != NIL"];
            break;
        case X_DATABASE:
            // No fetchRequest - we want all of them
            break;
        default:
            NSLog(@"exportMetadataToFile: Unhandled switch case!");
            break;
    }

    NSError *error = nil;
    NSArray *metadata = [context executeFetchRequest:fetchRequest error:&error];

    if (!metadata) {
        NSLog(@"exportMetadataToFile: Could not fetch metadata list. Error: %@", error);
    }

    if (metadata.count == 0) {
        fclose(fp);
        NSAlert *alert = [NSAlert new];
        alert.messageText = NSLocalizedString(@"No metadata found", nil);
        alert.informativeText = NSLocalizedString(@"No matching metadata to export was found in the library.", nil);
        [alert runModal];
        return NO;
    }

    enumerator = [metadata objectEnumerator];
    while ((meta = [enumerator nextObject]))
    {
        fprintf(fp, "<story>\n");

        fprintf(fp, "<identification>\n");
        for (Ifid *ifid in meta.ifids) {
            fprintf(fp, "<ifid>%s</ifid>\n", ifid.ifidString.UTF8String);
        }
        write_xml_text(fp, meta, @"format");
        write_xml_text(fp, meta, @"bafn");
        fprintf(fp, "</identification>\n");

        fprintf(fp, "<bibliographic>\n");

        write_xml_text(fp, meta, @"title");
        write_xml_text(fp, meta, @"author");
        write_xml_text(fp, meta, @"language");
        write_xml_text(fp, meta, @"headline");
        write_xml_text(fp, meta, @"firstpublished");
        write_xml_text(fp, meta, @"genre");
        write_xml_text(fp, meta, @"group");
        write_xml_text(fp, meta, @"series");
        write_xml_text(fp, meta, @"seriesnumber");
        write_xml_text(fp, meta, @"forgiveness");
        write_xml_text(fp, meta, @"blurb");

        fprintf(fp, "</bibliographic>\n");

        if (meta.cover) {
            NSData *data = (NSData *)meta.cover.data;
            NSString *type = @"png";
            if (data.JPEG)
                type = @"jpg";
            else if (!data.PNG)
                NSLog(@"Illegal image type!");

            NSImage *image = [[NSImage alloc] initWithData:data];
            NSString *description = meta.coverArtDescription;
            if (!description.length) {
                description = meta.cover.imageDescription;
            }
            if (!description.length) {
                description = @"";
            }
            fprintf(fp, "<cover>\n");
            fprintf(fp, "<format>%s</format>\n", type.UTF8String);
            fprintf(fp, "<height>%ld</height>\n", (NSInteger)image.size.height);
            fprintf(fp, "<width>%ld</width>\n", (NSInteger)image.size.width);
            fprintf(fp, "<description>%s</description>\n", description.UTF8String);
            fprintf(fp, "</cover>\n");
        }
        fprintf(fp, "</story>\n\n");
    }

    fprintf(fp, "</ifindex>\n");

    fclose(fp);
    return YES;
}

#pragma mark - Ask to Download

- (void)askToDownloadInWindow:(NSWindow *)win context:(NSManagedObjectContext *)ctx {
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"HasAskedToDownload"])
        return;

    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [Game fetchRequest];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hasDownloaded = YES"];

    fetchedObjects = [ctx executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"askToDownload: Could not fetch Game entities: %@",error);
        return;
    }

    if (fetchedObjects.count == 0)
    {
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:ctx];
        fetchRequest.predicate = nil;
        fetchRequest.includesPropertyValues = NO;
        fetchedObjects = [ctx executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects.count < 5)
            return;

        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasAskedToDownload"];

        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Do you want to download game info from IFDB in the background?", nil);
        alert.informativeText = NSLocalizedString(@"This will only be done once. You can do this later by selecting games in the library and choosing Download Info from the contextual menu.", nil);
        [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        MetadataHandler * __weak weakSelf = self;
        [alert beginSheetModalForWindow:win completionHandler:^(NSModalResponse result) {
            if (result == NSAlertFirstButtonReturn) {
                [weakSelf downloadMetadataForGames:fetchedObjects];
            }
        }];
    }
}

#pragma mark - Download Metadata

- (void)downloadMetadataForGames:(NSArray<Game *> *)games {

    TableViewController *tvc = self.tableViewController;

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    tvc.downloadWasCancelled = NO;

    [tvc.managedObjectContext performBlockAndWait:^{
        [tvc.managedObjectContext.undoManager beginUndoGrouping];
        tvc.undoGroupingCount++;
    }];

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

    tvc.downloadWasCancelled = NO;

    [tvc beginImporting];

    NSManagedObjectContext *childContext = tvc.coreDataManager.privateChildManagedObjectContext;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    [tvc.coreDataManager saveChanges];

    tvc.lastImageComparisonData = nil;

    // This deselects all games
    if (tvc.gameTableView.selectedRowIndexes.count > 10 && tvc.gameTableView.selectedRowIndexes.count == tvc.gameTableModel.count)
        [tvc.gameTableView selectRowIndexes:[NSIndexSet new] byExtendingSelection:NO];

    tvc.verifyIsCancelled = YES;
    tvc.currentlyAddingGames = YES;

    NSMutableArray<NSManagedObjectID *> __block *blockGames = [NSMutableArray new];

    for (Game *gameInMain in games) {
        [blockGames addObject:gameInMain.objectID];
    }

    TableViewController * __weak weakTVC = tvc;

    [childContext performBlock:^{

        IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithTableViewController:weakTVC];
        NSOperationQueue *queue = weakTVC.downloadQueue;

        // A block that will run when all
        // all metadata and all images are downloaded
        void (^finisherBlock)(void) = ^void() {
            weakTVC.currentlyAddingGames = NO;
            [childContext performBlock:^{
                for (NSManagedObjectID *objectID in blockGames) {
                    Game *game = [childContext objectWithID:objectID];
                    game.hasDownloaded = YES;
                }
                [childContext safeSave];
            }];
            [weakTVC endImporting];
            [weakTVC.coreDataManager saveChanges];
        };

        BOOL reportFailure = NO;
        if (blockGames.count == 1) {
            Game *game = [childContext objectWithID:blockGames.firstObject];
            if (game.metadata.ifids.count <= 1 || game.metadata.tuid.length)
                reportFailure = YES;
        }

        NSBlockOperation *finisher = [NSBlockOperation blockOperationWithBlock:finisherBlock];
        finisher.qualityOfService = NSQualityOfServiceUtility;

        NSOperation *lastOperation = [downloader downloadMetadataForGames:blockGames inContext:childContext onQueue:weakTVC.downloadQueue imageOnly:NO reportFailure:reportFailure completionHandler:^{
            NSOperation *op = weakTVC.lastImageDownloadOperation;
            if (op) {
                [finisher addDependency:op];
            }
            [queue addOperation:finisher];
        }];

        // This is probably unnecessary
        if (lastOperation) {
            [finisher addDependency:lastOperation];
        }
    }];
}

#pragma mark - Spotlight

- (void)handleSpotlightSearchResult:(id)object {
    Metadata *meta = nil;
    Game *game = nil;

    TableViewController *tvc = self.tableViewController;

    NSManagedObjectContext *context = [object managedObjectContext];

    if (!context)
        return;

    if (context.hasChanges)
        [context save:nil];

    if ([object isKindOfClass:[Metadata class]]) {
        meta = (Metadata *)object;
        game = meta.game;
    } else if ([object isKindOfClass:[Game class]]) {
        game = (Game *)object;
        meta = game.metadata;
    } else if ([object isKindOfClass:[Image class]]) {
        Image *image = (Image *)object;
        meta = image.metadata.anyObject;
        game = meta.game;
    } else {
        NSLog(@"Object: %@", [object class]);
    }

    if (game && [tvc.gameTableModel indexOfObject:game] == NSNotFound) {
        tvc.windowController.searchField.stringValue = @"";
        [tvc searchForGames:nil];
    }

    if (game) {
        [tvc.gameLauncher selectAndPlayGame:game];
    } else if (meta) {
        [self createDummyGameForMetadata:meta];
    }
}

- (void)createDummyGameForMetadata:(Metadata *)metadata {

    TableViewController *tvc = self.tableViewController;

    Game *game = [NSEntityDescription
                  insertNewObjectForEntityForName:@"Game"
                  inManagedObjectContext:metadata.managedObjectContext];

    game.ifid = metadata.ifids.anyObject.ifidString;
    game.metadata = metadata;
    game.added = [NSDate date];
    game.detectedFormat = metadata.format;
    game.found = NO;

    [tvc updateTableViews];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        [tvc selectGames:[NSSet setWithObject:game]];

        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:@"ShowSideBar" object:nil]];
    });
}

@end

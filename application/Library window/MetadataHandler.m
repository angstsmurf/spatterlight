//
//  MetadataHandler.m
//  Spatterlight
//
//  Extracted from TableViewController.m
//

#import "MetadataHandler.h"

#import "TableViewController.h"
#import "LibHelperTableView.h"
#import "TableViewController+Internal.h"
#import "TableViewController+TableDelegate.h"

#import "GameLauncher.h"
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
#import "Metadata+IFiction.h"
#import "IFIdentification.h"
#import "IFDBDownloader.h"

#import "CoreDataManager.h"
#import "NSAlert+Spatterlight.h"

enum { X_EDITED, X_LIBRARY, X_DATABASE }; // export selections

typedef NS_ENUM(int32_t, MetadataImportResult) {
    kImportResultNoneFound,
    kImportResultExactMatchesOnly,
    kImportResultPartialMatchesOnly,
    kImportResultExactAndPartialMatches
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

    if (!ifictionmetadata)
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
                if (story)
                    [game.metadata addInfoFromStory:story];
                game.metadata.source = @(kExternal);
                game.metadata.lastModified = [NSDate date];
            }
            [childContext safeSave];
        }];
    }];
}

- (void)askAboutImportingMetadata:(NSDictionary<NSString *, IFStory *> *)storyDict indirectMatches:(NSDictionary<NSString *, IFStory *> *)indirectDict {

    MetadataImportResult __block importResult = kImportResultNoneFound;

    NSMutableSet<Game *> *games = [NSMutableSet new];
    NSMutableSet<Game *> *indirectGames = [NSMutableSet new];
    NSMutableSet<NSManagedObjectID *> *gameIDs = [NSMutableSet new];
    NSMutableSet<NSManagedObjectID *> *indirectGameIDs = [NSMutableSet new];

    NSString __block *msg = @"There were no library matches for the games in the iFiction file.";
    NSString __block *msg2;


    TableViewController *tvc = self.tableViewController;
    NSManagedObjectContext *childContext = tvc.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    [childContext performBlockAndWait:^{
        // The keys are Game URIs from before the user was asked: a game may
        // have been deleted since, and a malformed URI gives a nil ID.
        Game *(^gameForURI)(NSString *, NSManagedObjectID **) = ^Game *(NSString *identifier, NSManagedObjectID **objectID) {
            NSURL *uri = [NSURL URLWithString:identifier];
            *objectID = uri ? [childContext.persistentStoreCoordinator managedObjectIDForURIRepresentation:uri] : nil;
            return *objectID ? (Game *)[childContext existingObjectWithID:*objectID error:NULL] : nil;
        };

        for (NSString *identifier in storyDict.allKeys) {
            NSManagedObjectID *objectID;
            Game *result = gameForURI(identifier, &objectID);
            if (result) {
                [games addObject:result];
                [gameIDs addObject:objectID];
            }
        }

        for (NSString *identifier in indirectDict.allKeys) {
            NSManagedObjectID *objectID;
            Game *result = gameForURI(identifier, &objectID);
            if (result && ![games containsObject:result]) {
                [indirectGames addObject:result];
                [indirectGameIDs addObject:objectID];
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
                importResult = kImportResultExactAndPartialMatches;
            else
                importResult = kImportResultPartialMatchesOnly;

            msg2 = [NSString stringWithFormat:@"%@ound partial match%@ for %@%@.", importResult == kImportResultExactAndPartialMatches ? @"Also f" : @"F", indirectGames.count > 1 ? @"es" : @"", [NSString stringWithSummaryOfGames:indirectGames.allObjects], importResult == kImportResultExactAndPartialMatches ? @"" : @" in iFiction file"];
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
        BOOL found = (importResult != kImportResultNoneFound);
        NSAlert *anAlert = [NSAlert alertWithMessage:NSLocalizedString(msg, nil)
                                                info:found ? NSLocalizedString(@"Would you like to replace existing information?", nil) : nil
                                             buttons:found ? @[ NSLocalizedString(@"Okay", nil), NSLocalizedString(@"Cancel", nil) ] : @[ NSLocalizedString(@"Okay", nil) ]];

        [anAlert beginSheetModalForWindow:tvc.view.window completionHandler:^(NSModalResponse result){
            if (importResult == kImportResultNoneFound)
                return;
            if (result == NSAlertFirstButtonReturn) {
                if (importResult == kImportResultExactMatchesOnly ||
                    importResult == kImportResultExactAndPartialMatches) {
                    [weakSelf addInfoToMetadata:gameIDs fromStories:storyDict];
                } else if (importResult == kImportResultPartialMatchesOnly) {
                    [weakSelf addInfoToMetadata:indirectGameIDs fromStories:indirectDict];
                }
            }
            if (importResult == kImportResultExactAndPartialMatches) {
                NSAlert *alert2 = [NSAlert alertWithMessage:NSLocalizedString(msg2, nil)
                                                       info:anAlert.informativeText
                                                    buttons:@[ NSLocalizedString(@"Okay", nil), NSLocalizedString(@"Cancel", nil) ]];
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

//! Writes <tagname>text</tagname>, with &, < and > escaped and runs of
//! newlines turned into <br/>.
static void write_xml_element(FILE *fp, NSString *key, NSString *text) {
    const char *tagname = key.UTF8String;
    const char *s = text.UTF8String;
    if (!s)
        s = "";

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

static void write_xml_text(FILE *fp, Metadata *info, NSString *key) {
    id val = [info valueForKey:key];
    if (!val)
        return;

    // "description" is a reserved word in core data,
    // so we use "blurb" internally. We change this back
    // to "description" in XML output.
    if ([key isEqualToString:@"blurb"])
        key = @"description";

    write_xml_element(fp, key, [NSString stringWithFormat:@"%@", val]);
}

- (BOOL)exportMetadataToFile:(NSString *)filename what:(NSInteger)what context:(NSManagedObjectContext *)context {
    NSEnumerator *enumerator;
    Metadata *meta;


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
        [[NSAlert alertWithMessage:NSLocalizedString(@"No metadata found", nil)
                              info:NSLocalizedString(@"No matching metadata to export was found in the library.", nil)
                           buttons:nil] runModal];
        return NO;
    }

    enumerator = [metadata objectEnumerator];
    while ((meta = [enumerator nextObject]))
    {
        fprintf(fp, "<story>\n");

        fprintf(fp, "<identification>\n");
        for (Ifid *ifid in meta.ifids) {
            write_xml_element(fp, @"ifid", ifid.ifidString);
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
            write_xml_element(fp, @"description", description);
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
    fetchRequest.fetchLimit = 1;

    NSUInteger downloadedCount = [ctx countForFetchRequest:fetchRequest error:&error];
    if (downloadedCount == NSNotFound) {
        NSLog(@"askToDownload: Could not fetch Game entities: %@",error);
        return;
    }

    if (downloadedCount == 0)
    {
        fetchRequest.fetchLimit = 0;
        fetchRequest.predicate = nil;
        fetchRequest.includesPropertyValues = NO;
        fetchedObjects = [ctx executeFetchRequest:fetchRequest error:&error];

        if (fetchedObjects.count < 5)
            return;

        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"HasAskedToDownload"];

        NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Do you want to download game info from IFDB in the background?", nil)
                                              info:NSLocalizedString(@"This will only be done once. You can do this later by selecting games in the library and choosing Download Info from the contextual menu.", nil)
                                           buttons:@[NSLocalizedString(@"Okay", nil), NSLocalizedString(@"Cancel", nil)]];

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
     postNotification:[NSNotification notificationWithName:SpatterlightStopIndexingNotification object:nil]];

    tvc.downloadWasCancelled = NO;

    [tvc.managedObjectContext performBlockAndWait:^{
        [tvc.managedObjectContext.undoManager beginUndoGrouping];
        tvc.undoGroupingCount++;
    }];

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

    NSMutableArray<NSManagedObjectID *> *blockGames = [NSMutableArray new];

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
                [childContext safeSaveAndWait];
                // Only now are the child's changes in the main context
                [weakTVC.coreDataManager saveChanges];
            }];
            [weakTVC endImporting];
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

    // selectAndPlayGame: clears the search bar if it hides the game.
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

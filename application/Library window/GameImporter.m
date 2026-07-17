//
//  GameImporter.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-27.
//
#import <BlorbFramework/BlorbFramework.h>

#import "GameImporter.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "Fetches.h"

#import "CoreDataManager.h"

#import "IFDBDownloader.h"

#import "NSString+Categories.h"
#import "NSData+Categories.h"
#import "OSImageHashing.h"
#import "Preferences.h"
#import "Constants.h"

// Treaty of babel header
#include "babel_handler.h"
#import "IFictionMetadata.h"
#import "IFStory.h"

#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "MetadataHandler.h"

#import "FolderAccess.h"
#import "NSManagedObjectContext+safeSave.h"
#import "ImageCompareViewController.h"
#import "ComparisonOperation.h"
#import "LibraryOrganizer.h"

extern NSArray *gGameFileTypes;

// Computes an AGT game's Treaty-of-Babel IFID directly from its .D$$/.DA1
// data files, in-process, without converting to AGX first. Implemented in
// terps/agility/agtifid.c (linked into the app). Returns a malloc'd string
// the caller frees, or NULL if the file set is not a readable AGT game.
extern char *agt_copy_ifid_for_file(const char *path);

// agt_copy_ifid_for_file() runs the AGT reader, which uses global state and is
// not thread-safe. Imports and the AGT migration can run on different
// background contexts, so funnel every call through one process-wide lock.
static NSString *AGTIfidForPath(NSString *path) {
    @synchronized ([GameImporter class]) {
        char *raw = agt_copy_ifid_for_file(path.fileSystemRepresentation);
        if (!raw)
            return nil;
        NSString *ifid = @(raw);
        free(raw);
        return ifid.length ? ifid : nil;
    }
}

@implementation GameImporter

- (instancetype)initWithTableViewController:(TableViewController *)tableViewController {
    self = [super init];
    if (self) {
        _tableViewController = tableViewController;
    }
    return self;
}

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options {

    NSMutableArray *select = nil;

    //Don't select every game after import if we start with no games.
    //Check the whole library, not gameTableModel.count: the latter is the
    //search-filtered view, so an active search that matches nothing would
    //otherwise leave select nil and skip selectGamesWithHashes: entirely
    //(which is also what clears a search bar hiding the imported game).
    NSFetchRequest *countRequest = [NSFetchRequest fetchRequestWithEntityName:@"Game"];
    countRequest.predicate = [NSPredicate predicateWithFormat:@"hidden == NO"];
    NSUInteger libraryCount = [_tableViewController.managedObjectContext countForFetchRequest:countRequest error:NULL];
    if (libraryCount != NSNotFound && libraryCount > 0)
        select = [NSMutableArray arrayWithCapacity:urls.count];

    BOOL reportFailure = NO;

    NSUInteger numberOfFiles = urls.count;

    if (numberOfFiles == 1) {
        // If the user only selects one file, and it is
        // not a directory, we should report any failures.
        BOOL isDir;
        [[NSFileManager defaultManager] fileExistsAtPath:((NSURL *)urls[0]).path isDirectory: &isDir];
        if (!isDir)
            reportFailure = YES;
    }

    NSMutableDictionary *newOptions = options.mutableCopy;
    newOptions[@"reportFailure"] = @(reportFailure);
    newOptions[@"select"] = select;

    TableViewController *tableViewController = _tableViewController;
    NSManagedObjectContext __block *context = options[@"context"];

    // A block that will run when all files are added
    // and all metadata is downloaded
    void (^internalHandler)(void) = ^void() {
        [context performBlock:^{
            [context safeSave];

            tableViewController.currentlyAddingGames = NO;

            [FolderAccess releaseBookmark:[FolderAccess suitableDirectoryForURL:urls.firstObject]];

            [tableViewController.coreDataManager saveChanges];
            if (tableViewController.ifictionMatches.count ||
                tableViewController.ifictionPartialMatches.count) {
                [tableViewController askAboutImportingMetadata:tableViewController.ifictionMatches indirectMatches:tableViewController.ifictionPartialMatches];
                tableViewController.ifictionMatches = [[NSMutableDictionary alloc] init];
                tableViewController.ifictionPartialMatches = [[NSMutableDictionary alloc] init];
            }

            [tableViewController endImporting];
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
                [tableViewController selectGamesWithHashes:select scroll:YES clearSearchIfHidden:YES];
            });
        }];

    }; // End of the block

    newOptions[@"completionHandler"] = internalHandler;

    [tableViewController beginImporting];
    [self addGamesFromURLsRecursively:urls options:newOptions];
}


- (nullable NSOperation *)addGamesFromURLsRecursively:(NSArray*)urls options:(NSDictionary *)options {
    NSManagedObjectContext *context = options[@"context"];
    void (^internalHandler)(void) = options[@"completionHandler"];

    // Prevent recursive calls from re-running the completion handler
    if (internalHandler) {
        NSMutableDictionary *mutableOptions = options.mutableCopy;
        mutableOptions[@"completionHandler"] = nil;
        options = mutableOptions;
    }

    NSFileManager *filemgr = [NSFileManager defaultManager];
    NSDate *timestamp = [NSDate date];

    NSOperation *lastOperation = nil;
    TableViewController *tableViewController = _tableViewController;

    for (NSURL *url in urls) {
        if (!tableViewController.currentlyAddingGames) {
            if (internalHandler)
                internalHandler();
            return nil;
        }

        NSNumber *isDirectory = nil;
        [url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];

        if (isDirectory.boolValue) {
            NSArray *contentsOfDir = [filemgr contentsOfDirectoryAtURL:url
                                            includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                                                               options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                 error:nil];
            lastOperation = [self addGamesFromURLsRecursively:contentsOfDir options:options];
        } else {
            lastOperation = [self addSingleFile:url options:options];
        }

        if (timestamp.timeIntervalSinceNow < -0.3) {
            [context safeSaveAndWait];
            [_tableViewController.coreDataManager saveChanges];
            timestamp = [NSDate date];
        }
    }

    if (internalHandler) {
        NSBlockOperation *finisher = [NSBlockOperation blockOperationWithBlock:internalHandler];
        if (lastOperation)
            [finisher addDependency:lastOperation];
        [tableViewController.downloadQueue addOperationWithBlock:^{
            NSOperation *op = tableViewController.lastImageDownloadOperation;
            if (op)
                [finisher addDependency:op];
            [tableViewController.downloadQueue addOperation:finisher];
        }];
        lastOperation = nil; // This ends recursion
    }
    return lastOperation;
}

- (nullable NSOperation *)addSingleFile:(NSURL*)url options:(NSDictionary *)options
{
    NSMutableArray *select = options[@"select"];
    BOOL reportFailure = [options[@"reportFailure"] isEqual:@YES];
    BOOL lookForImages = [options[@"lookForImages"] isEqual:@YES];
    BOOL downloadInfo = [options[@"downloadInfo"] isEqual:@YES];
    NSManagedObjectContext *context = options[@"context"];
    options = nil;

    NSOperation *lastOperation = nil;

    Game *game = [self importGame:url.path inContext:context reportFailure:reportFailure hide:NO];

    if (game) {
        [_tableViewController beginImporting];
        if (select)
            [select addObject:game.hashTag];
        // Download metadata from IFDB here if the option to do this is active
        if (downloadInfo) {
            IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithTableViewController:_tableViewController];
            NSManagedObjectID *gameID = game.objectID.copy;
            lastOperation = [downloader downloadMetadataForGames:@[gameID] inContext:context onQueue:_tableViewController.downloadQueue imageOnly:NO reportFailure:NO completionHandler:^{
                [context performBlock:^{
                    Game *blockGame = [context objectWithID:gameID];
                    blockGame.hasDownloaded = YES;
                }];
            }];
        }
        // We only look for images on the HDD if the game has
        // no cover image or if it has the Inform 7 placeholder image.
        if (lookForImages && (game.metadata.cover.data == nil || [(NSData *)game.metadata.cover.data isPlaceHolderImage]))
            [self lookForImagesForGame:game];
    } else {
//        NSLog(@"libctl: addFile: File %@ not added!", url.path);
    }
    return lastOperation;
}

void freeContext(void **ctx) {
    babel_release_ctx(*ctx);
    free(*ctx);
    *ctx = nil;
}

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide {
    char buf[TREATY_MINIMUM_EXTENT];
    Metadata __block *metadata;
    Game __block *game;
    Blorb __block *blorb = nil;
    NSString *ifid;
    NSString *formatStr = nil;
    char *format = NULL;
    char *s;
    int rv;

    NSString *extension = path.pathExtension.lowercaseString;

    // AGT games are played directly from their original .D$$/.DA1 file set,
    // with no AGX conversion. We identify the game in-process and leave the
    // path pointing at the original .D$$ -- the agility terp loads the file
    // set in place (its siblings are reachable through the folder-level
    // sandbox access granted at launch). babel only understands AGX, so for
    // the direct path we skip the babel identification below.
    BOOL directAGT = NO;

    if ([extension isEqualToString: @"d$$"]) {
        ifid = AGTIfidForPath(path);
        if (!ifid) {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Could not read game.", nil);
                    alert.informativeText = NSLocalizedString(@"This file could not be read as an AGT game.", nil);
                    [alert runModal];
                });
            } else {
                NSLog(@"This file could not be read as an AGT game.");
            }
            return nil;
        }
        formatStr = @"agt";
        format = (char *)"agt";
        directAGT = YES;
    }

    if (![gGameFileTypes containsObject:extension])
    {
            if ([extension isEqualToString:@"ifiction"]) {
                NSData *data = [NSData dataWithContentsOfFile:path];
                if (data) {
                    NSDictionary<NSString *, IFStory *> *indirectMatches;
                    NSDictionary<NSString *, IFStory *> *exactMatches = [MetadataHandler importMetadataFromXML:data indirectMatches:&indirectMatches inContext:context];
                    [_tableViewController.ifictionMatches addEntriesFromDictionary:exactMatches];
                    [_tableViewController.ifictionPartialMatches addEntriesFromDictionary:indirectMatches];
                    return nil;
                }
            }

        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Can not recognize the file extension \".%@\"", nil), path.pathExtension];
                [alert runModal];
            });
        }
        return nil;
    }

    NSData *fileData = nil;
    if ([extension isEqualToString:@"blorb"] || [extension isEqualToString:@"blb"] || [extension isEqualToString:@"zblorb"] || [extension isEqualToString:@"zbl"]) {
        fileData = [NSData dataWithContentsOfFile:path];
        blorb = [[Blorb alloc] initWithData:fileData];
        BlorbResource *executable = [blorb resourcesForUsage:ExecutableResource].firstObject;
        if (!executable) {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Not a game.", nil);
                    alert.informativeText = NSLocalizedString(@"No executable chunk found in Blorb file.", nil);
                    [alert runModal];
                });
//            } else {
//                NSLog(@"No executable chunk found in Blorb file.");
            }
            return nil;
        }

        // ADRIFT games shipped in a Blorb (an ADRI executable chunk) are
        // identified by the normal babel pass below: babel/blorb.c maps the
        // ADRI chunk to the "adrift" format and reads the IFID/metadata from
        // the IFmd chunk, and the scarier terp plays the Blorb directly.
    }

    if (!directAGT) {
        void *ctx = get_babel_ctx();
        char pathBuf[PATH_MAX];
        strlcpy(pathBuf, path.fileSystemRepresentation, sizeof(pathBuf));
        format = babel_init_ctx(pathBuf, ctx);
        if (!format || !babel_get_authoritative_ctx(ctx)) {
            freeContext(&ctx);
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                    alert.informativeText = NSLocalizedString(@"Babel can not identify the file format.", nil);
                    [alert runModal];
                });
            }
            return nil;
        }

        s = strchr(format, ' ');
        if (s) format = s+1;

        formatStr = @(format);

        rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, ctx);
        freeContext(&ctx);
        if (rv <= 0) {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Fatal error.", nil);
                    alert.informativeText = NSLocalizedString(@"Can not compute IFID from the file.", nil);
                    [alert runModal];
                });
            } else {
                NSLog(@"Babel can not compute IFID from the file.");
            }
            return nil;
        }

        s = strchr(buf, ',');
        if (s) *s = 0;
        ifid = @(buf);
    }

    NSString *terp = gExtMap[path.pathExtension.lowercaseString];

    if (!terp)
        terp = gFormatMap[formatStr];

    // Some extensions are used across many unrelated formats (e.g. .dsk could be a
    // Scott Adams disk image, a Level 9 game, a Taylor adventure, etc.). For each
    // such ambiguous extension, list the formats we actually know how to handle;
    // any other format reported by babel is rejected. This guards against, e.g.,
    // a random .dsk disk image being misidentified.
    static NSDictionary<NSString *, NSSet<NSString *> *> *allowedFormatsByExtension;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        allowedFormatsByExtension = @{
            @"dat": [NSSet setWithObjects:@"zcode", @"level9", @"advsys", @"sagaplus", @"scott", nil],
            @"gam": [NSSet setWithObjects:@"tads2", nil],
            @"msa": [NSSet setWithObjects:@"sagaplus", nil],
            @"st":  [NSSet setWithObjects:@"sagaplus", nil],
            @"d64": [NSSet setWithObjects:@"scott", @"taylor", @"sagaplus", @"level9", nil],
            @"dsk": [NSSet setWithObjects:@"scott", @"taylor", @"sagaplus", @"level9", @"comprehend", nil],
            @"woz": [NSSet setWithObjects:@"scott", @"sagaplus", @"comprehend", @"zcode", nil],
            @"exe": [NSSet setWithObjects:@"comprehend", nil],
        };
    });

    NSSet<NSString *> *allowedFormats = allowedFormatsByExtension[extension];
    BOOL extensionIsAmbiguous = (allowedFormats != nil);
    BOOL formatMatchesExtension = [allowedFormats containsObject:formatStr];

    // .dat + zcode additionally requires the file to actually look like Z-code,
    // since plenty of unrelated games ship a story file called e.g. "story.dat".
    if (formatMatchesExtension && [extension isEqualToString:@"dat"] &&
        [formatStr isEqualToString:@"zcode"] && ![self checkZcode:path]) {
        formatMatchesExtension = NO;
    }

    if (!terp || (extensionIsAmbiguous && !formatMatchesExtension)) {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = NSLocalizedString(@"Not a supported format.", nil);
                [alert runModal];
            });
//        } else {
//            NSLog(@"%@: Recognized extension (%@) but unknown file format.", path.lastPathComponent, extension);
        }
        return nil;
    }

//    NSLog(@"GameImporter: import game %@ (%@)", path, formatStr);

    if (ifid == nil) {
        // If this happens, it means the Babel tool did not
        // work as it should. It detected the game but returned
        // an invalid IFID.
        NSLog(@"Error! Ifid nil! buf:%s (%x%x%x%x)", buf, buf[0], buf[1], buf[2], buf[3]);
    }

    NSData __block *blockdata = fileData;
    NSString *hash = path.signatureFromFile;

    if (!hash.length)
        return nil;

    [context performBlockAndWait:^{
        // We really should check if there is a game-less metadata object that we can use here
        // but we skip this for now.
        metadata = [Fetches fetchMetadataForHash:hash inContext:context];
        game = metadata.game;

        if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path isDirectory:NO]] && !blorb) {
            if (blockdata == nil)
                blockdata = [NSData dataWithContentsOfFile:path];
            blorb = [[Blorb alloc] initWithData:blockdata];
        }

        if (!game) {
//            NSLog(@"importGame: Creating new Game object for game with hash %@", hash);
            game = (Game *) [NSEntityDescription
                             insertNewObjectForEntityForName:@"Game"
                             inManagedObjectContext:context];
        } else {
//            NSLog(@"importGame: Game with hash %@ already exists in library (%@)", hash, path.lastPathComponent);
        }

        if (!metadata) {
//            NSLog(@"GameImporter importGame: Creating new Metadata object for game with hash %@", hash);
            metadata = (Metadata *) [NSEntityDescription
                                     insertNewObjectForEntityForName:@"Metadata"
                                     inManagedObjectContext:context];
        }

        // There can be any number of Ifid objects with the same ifid string (because we ambitiously want to keep track of all ifids associated with a game) but they must all be attached to a Metadata object
        [metadata createIfid:ifid];

        if (!metadata.format)
            metadata.format = formatStr;
        if (metadata.title.length == 0) {
            metadata.title = path.lastPathComponent;
        }

        if (blorb.metaData) {
            // ADRIFT (5) blorbs frequently carry a placeholder
            // <title>Untitled</title> in their embedded iFiction metadata.
            // Don't let that clobber the filename-derived (or previously
            // stored) title.
            NSString *titleBeforeXML = metadata.title;
            [GameImporter importInfoFromXML:blorb.metaData intoMetadata:metadata];
            if ([formatStr isEqualToString:@"adrift"] &&
                [metadata.title caseInsensitiveCompare:@"Untitled"] == NSOrderedSame) {
                metadata.title = titleBeforeXML;
            }
            metadata.source = @(kInternal);
            metadata.lastModified = [NSDate date];
        }

        [game bookmarkForPath:path];

        game.added = [NSDate date];
        game.hidden = hide;
        game.ifid = ifid;
        game.detectedFormat = @(format);
        game.hashTag = hash;
        metadata.hashTag = hash;

        if (!metadata.cover && blorb) {
            NSData *imageData = blorb.coverImageData;
            if (imageData) {
                metadata.coverArtURL = path;
                [IFDBDownloader insertImageData:imageData inMetadataID:metadata.objectID context:context];
                //                    NSLog(@"Extracted cover image from blorb for game %@", metadata.title);
            }
            //                else NSLog(@"Found no image in blorb file %@", path);
        }

        game.metadata = metadata;

        if ([game.detectedFormat isEqualToString:@"zcode"]) {
            // Used when associating save file with game
            [self addZCodeIDfromFile:path blorb:blorb toGame:game];
        } else if ([game.detectedFormat isEqualToString:@"glulx"]) {
            // Used when associating save file with game
            game.serialString = [path oldSignatureFromFile];
        }
        blorb = nil;
//        NSLog(@"GameImporter importGame: Title: %@ Hash:%@", game.metadata.title, game.hashTag);

        // If the user asked us to keep games organised, copy this newly
        // imported game into the central library folder and re-point its
        // bookmark. Skip hidden games (opened but not added to the library).
        // The source folder is already access-granted for the whole import run.
        if (!hide && [LibraryOrganizer keepGamesOrganised]) {
            NSError *orgError = nil;
            if (![[LibraryOrganizer sharedOrganizer] organiseGame:game error:&orgError])
                NSLog(@"GameImporter: could not organise \"%@\": %@",
                      metadata.title, orgError.localizedDescription);
        }
    }];

    return game;
}

+ (void)importInfoFromXML:(NSData *)mdbuf intoMetadata:(Metadata * _Nonnull)metadata {
    IFictionMetadata *ifictionmetadata = [[IFictionMetadata alloc] initWithData:mdbuf];
    if (!ifictionmetadata || ifictionmetadata.stories.count == 0)
        return;
    if (ifictionmetadata.stories.count > 1) {
        NSLog(@"Found more than one story in data (%ld). Only using the first one", ifictionmetadata.stories.count);
    }
    IFStory *story = ifictionmetadata.stories.firstObject;
    [story addInfoToMetadata:metadata];
}

- (void)updateImageFromBlorb:(Blorb *)blorb inGame:(Game *)game {
    if (blorb) {
        NSData *newImageData = blorb.coverImageData;
        kImageReplacementPrefsType userSetting = (kImageReplacementPrefsType)[[NSUserDefaults standardUserDefaults] integerForKey:@"ImageReplacement"];
        if (newImageData && ![newImageData isPlaceHolderImage] &&
            (userSetting == kAlwaysReplace || ![_tableViewController.lastImageComparisonData isEqual:newImageData])) {
            NSData *oldImageData = (NSData *)game.metadata.cover.data;
            if (oldImageData && blorb.fakeFrontispiece)
                return;
            _tableViewController.lastImageComparisonData = newImageData;
            kImageComparisonResult comparisonResult = [ImageCompareViewController chooseImageA:newImageData orB:oldImageData source:kImageComparisonDownloaded force:NO];
            if (comparisonResult != kImageComparisonResultB) {
                if (comparisonResult == kImageComparisonResultWantsUserInput) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        ImageCompareViewController *imageCompare = [[ImageCompareViewController alloc] initWithNibName:@"ImageCompareViewController" bundle:nil];
                        if ([imageCompare userWantsImage:newImageData ratherThanImage:oldImageData source:kImageComparisonLocalFile force:NO]) {
                            [IFDBDownloader insertImageData:newImageData inMetadataID:game.metadata.objectID context:game.managedObjectContext];
                        }
                    });
                } else {
                    [IFDBDownloader insertImageData:newImageData inMetadataID:game.metadata.objectID context:game.managedObjectContext];
                }
            }
        }
    }
}

- (void)addZCodeIDfromFile:(NSString *)path blorb:(nullable Blorb *)blorb toGame:(Game *)game {
    BOOL found = NO;
    NSData *data = nil;
    if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path isDirectory:NO]]) {
        if (!blorb)
            blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];
        if (blorb.checkSum && blorb.serialNumber && blorb.releaseNumber) {
            found = YES;
            game.checksum = (int32_t)blorb.checkSum;
            game.serialString = blorb.serialNumber;
            game.releaseNumber = (int32_t)blorb.releaseNumber;
        }
        if (!found) {
            BlorbResource *zcode = [blorb resourcesForUsage:ExecutableResource].firstObject;
            if (zcode)
                data = [blorb dataForResource:zcode];
        }
    }
    if (!found) {
        if (!data)
            data = [NSData dataWithContentsOfFile:path];
        const unsigned char *ptr = data.bytes;
        game.releaseNumber = shortAtPointer(ptr + 2);
        NSData *serialData = [NSData dataWithBytes:ptr + 18 length:6];
        game.serialString = [[NSString alloc] initWithData:serialData encoding:NSASCIIStringEncoding];
        game.checksum = shortAtPointer(ptr + 28);
    }
}

int32_t shortAtPointer(const void *data) {
    return (((const unsigned char *)data)[0] << 8) |
    (((const unsigned char *)data)[1]);
}

static inline uint16_t word(uint8_t *memory, uint32_t addr)
{
    return (uint16_t)((memory[addr] << 8) | memory[addr + 1]);
}

- (BOOL)checkZcode:(NSString *)file {
    NSData *mem = [NSData dataWithContentsOfFile:file];
    uint8_t *memory = (uint8_t *)mem.bytes;

    uint16_t dictionary = word(memory, 0x08);
    uint32_t static_start = word(memory, 0x0e);

    // corrupted story: dictionary is not in static memory
    if (dictionary != 0 && dictionary < static_start) {
        return NO;
    }

    uint16_t objects = word(memory, 0x0a);
    int zversion = memory[0x00];
    unsigned long propsize = (zversion <= 3 ? 62UL : 126UL);

    // corrupted story: object table is not in dynamic memory
    if(objects < 64 ||
       objects + propsize > static_start) {
        return NO;
    }

    // corrupted story: dynamic memory too small
    if (static_start < 64UL + 480UL + propsize) {
        return NO;
    }
    return YES;
}

- (void)lookForImagesForGame:(Game *)game {
    //    NSLog(@"lookForImagesForGame %@", game.metadata.title);
    NSFileManager *filemgr = [NSFileManager defaultManager];
    NSURL *dirUrl = [NSURL fileURLWithPath:(game.path).stringByDeletingLastPathComponent isDirectory:YES];
    // First check if there are other games in this folder
    NSArray *contentsOfDir = [filemgr contentsOfDirectoryAtURL:dirUrl
                                    includingPropertiesForKeys:@[NSURLNameKey, NSURLIsDirectoryKey]
                                                       options:NSDirectoryEnumerationSkipsHiddenFiles
                                                         error:nil];

    NSMutableSet<NSString *> *gameNames = [[NSMutableSet alloc] initWithCapacity:contentsOfDir.count];

    NSURL *coverURL = nil;

    for (NSURL *url in contentsOfDir) {
        NSNumber *isDir;
        [url getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil];
        NSString *filename = nil;
        [url getResourceValue:&filename forKey:NSURLNameKey error:nil];
        if (isDir.boolValue && [filename isEqualToString:@"Cover"])
            coverURL = url;
        NSString *extension = filename.pathExtension.lowercaseString;
        if ([gGameFileTypes indexOfObject:extension] != NSNotFound) {
            [gameNames addObject:filename.stringByDeletingPathExtension.lowercaseString];
        }
    }

    // Then find all the image files in the same folder
    NSArray *gImageFileTypes = @[@"jpg", @"png", @"gif", @"jpeg", @"tiff", @"psd", @"tif", @"ico", @"mg1", @"neo", @"blorb", @"blb"];

    NSPredicate *imageFilesFilter = [NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
        NSString *name = nil;
        [(NSURL *)object getResourceValue:&name forKey:NSURLNameKey error:nil];
        NSString *extension = name.pathExtension.lowercaseString;
        if ([((NSURL *)object).path isEqualToString:game.path])
            return NO;
        return ([gImageFileTypes indexOfObject:extension] != NSNotFound);
    }];

    NSArray<NSURL *> *imageFiles = [contentsOfDir filteredArrayUsingPredicate:imageFilesFilter];

    // If there is more than one game in folder, and they don't share the same
    // file name without extension, i.e. "enchanter.z3" and "zork.z5" rather than
    // "zork.z3" and "zork.z5", we only look for image files containing the game file
    // name without extension ("zork.jpg" and "zork.mg1", but not "image.jpg" etc.)
    // In this way we don't give every game in folder of thousands the same cover image.
    if (gameNames.count > 1) {
        if (imageFiles.count) {
            NSString *gameFileName = game.path.lastPathComponent.stringByDeletingPathExtension.lowercaseString;
            imageFiles = [imageFiles filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                NSString *imgFileName = ((NSURL *)object).path.lastPathComponent.stringByDeletingPathExtension.lowercaseString;
                if (imgFileName.length > gameFileName.length)
                    imgFileName = [imgFileName substringToIndex:imgFileName.length - 1];
                return [imgFileName isEqualToString:gameFileName];
            }]];
        }

        if (!imageFiles.count &&[gameNames containsObject:@"screen"] && [gameNames containsObject:@"story"]) {
            // The original Beyond Zork cover image is named SCREEN.DAT, but is in fact
            // a Neochrome image. We cheat and create a renamed copy with a .neo file extension
            // (SCREEN.neo) in a temp folder.
            imageFiles = [self moveScreenDatFrom:game.path];
        }
    } else if (imageFiles.count == 0) {
        if (coverURL) {
            NSArray *coverDir = [filemgr contentsOfDirectoryAtURL:coverURL
                                       includingPropertiesForKeys:@[NSURLNameKey]
                                                          options:NSDirectoryEnumerationSkipsHiddenFiles
                                                            error:nil];
            coverDir = [coverDir filteredArrayUsingPredicate:imageFilesFilter];
            if (coverDir.count) {
                imageFiles = coverDir;
            }
        }

        if (imageFiles.count == 0) {
            NSString *currentDir = game.path.stringByDeletingLastPathComponent.lastPathComponent;
            if ([currentDir isEqualToString:@"Data"]) {
                dirUrl = [NSURL fileURLWithPath:game.path.stringByDeletingLastPathComponent.stringByDeletingLastPathComponent isDirectory:YES];
                contentsOfDir = [filemgr contentsOfDirectoryAtURL:dirUrl
                                       includingPropertiesForKeys:@[NSURLNameKey, NSURLIsDirectoryKey]
                                                          options:NSDirectoryEnumerationSkipsHiddenFiles
                                                            error:nil];
                imageFiles = [contentsOfDir filteredArrayUsingPredicate:imageFilesFilter];
            }
        }
    }

    if (imageFiles.count > 0) {
        NSData *imageData = nil;
        NSURL *chosenURL = nil;
        for (NSURL *url in imageFiles) {
            NSError *error = nil;

            imageData = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:&error];

            NSString *imgExtension = url.pathExtension.lowercaseString;

            if ([imgExtension isEqualToString:@"mg1"]) {
                imageData = [NSData imageDataFromMG1URL:imageFiles.firstObject];
            } else if ([imgExtension isEqualToString:@"neo"]) {
                imageData = [NSData imageDataFromNeoURL:imageFiles.firstObject];
            } else if ([imgExtension isEqualToString:@"blb"] || [imgExtension isEqualToString:@"blorb"]) {
                Blorb *blorb = [[Blorb alloc] initWithData:imageData];
                imageData = blorb.coverImageData;
            }
            if (imageData.length) {
                chosenURL = url;
                break;
            }
        }

        if (!imageData)
            return;

        game.metadata.coverArtURL = chosenURL.path;
        [IFDBDownloader insertImageData:imageData inMetadataID:game.metadata.objectID context:game.managedObjectContext];
    }
}


// Creates a renamed copy of SCREEN.DAT in a temporary directory
// and returns an array containing its URL so the cover image
// importing code recognizes it as a Neochrome image file

- (NSArray<NSURL *> *)moveScreenDatFrom:(NSString *)path {
    NSFileManager *filemanager = [NSFileManager defaultManager];

    NSError *error = nil;
    NSURL *desktopURL = [NSURL fileURLWithPath:path
                                   isDirectory:YES];


    NSURL *temporaryDirectoryURL = [filemanager
                                    URLForDirectory:NSItemReplacementDirectory
                                    inDomain:NSUserDomainMask
                                    appropriateForURL:desktopURL
                                    create:YES
                                    error:&error];

    NSString *tempFilePath = [temporaryDirectoryURL.path stringByAppendingPathComponent:[NSUUID UUID].UUIDString];

    NSString *screenDatPath = [path.stringByDeletingLastPathComponent
                               stringByAppendingPathComponent:@"SCREEN.DAT"];

    if ([filemanager fileExistsAtPath:screenDatPath]) {
        NSURL *oldURL = [NSURL fileURLWithPath:screenDatPath isDirectory:NO];
        NSString *newURLpath = [tempFilePath stringByAppendingPathExtension:@"neo"];
        NSURL *newURL =  [NSURL fileURLWithPath:newURLpath isDirectory:NO];

        [filemanager
         copyItemAtURL:oldURL
         toURL:newURL
         error:&error];
        return @[newURL];
    }
    return @[];
}

- (void)migrateConvertedAGTGamesInContext:(NSManagedObjectContext *)context {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults boolForKey:@"AGTConvertedToDirectMigrationDone"])
        return;

    NSFileManager *fm = NSFileManager.defaultManager;

    // Path of our container's Converted folder, used to recognise (and only
    // ever delete within) the old converted .agx files.
    NSString *convertedPath =
    [_tableViewController.homepath URLByAppendingPathComponent:@"Converted" isDirectory:YES].absoluteURL.path;

    // AGT games still stored as a converted .agx (direct ones are .d$$).
    NSArray<Game *> *agtGames =
    [Fetches fetchObjects:@"Game" predicate:@"detectedFormat == \"agt\"" inContext:context];

    NSMutableArray<Game *> *candidates = [NSMutableArray array];
    for (Game *game in agtGames) {
        if ([game.path.pathExtension.lowercaseString isEqualToString:@"agx"])
            [candidates addObject:game];
    }

    if (!candidates.count) {
        [defaults setBool:YES forKey:@"AGTConvertedToDirectMigrationDone"];
        return;
    }

    // Build a map of IFID -> original .D$$ by scanning every folder we still
    // hold access to (these include the folders the games were imported from).
    NSMutableDictionary<NSString *, NSURL *> *ifidToDss = [NSMutableDictionary dictionary];
    for (NSURL *folder in [FolderAccess bookmarkedFolders]) {
        NSURL *accessURL = [FolderAccess restoreURL:folder];
        if (!accessURL)
            continue;
        NSArray<NSURL *> *contents =
        [fm contentsOfDirectoryAtURL:accessURL
          includingPropertiesForKeys:nil
                             options:NSDirectoryEnumerationSkipsHiddenFiles
                               error:nil];
        for (NSURL *fileURL in contents) {
            if (![fileURL.pathExtension.lowercaseString isEqualToString:@"d$$"])
                continue;
            NSString *foundIfid = AGTIfidForPath(fileURL.path);
            if (foundIfid && !ifidToDss[foundIfid])
                ifidToDss[foundIfid] = fileURL;
        }
        [FolderAccess releaseBookmark:folder];
    }

    for (Game *game in candidates) {
        NSURL *dssURL = ifidToDss[game.ifid];
        if (!dssURL || ![fm isReadableFileAtPath:dssURL.path])
            continue;  // original not found; leave this game on the AGX path

        NSString *oldAGX = game.path;
        [game bookmarkForPath:dssURL.path];
        game.found = YES;

        // Remove the now-orphaned converted file (and its copied icon), but
        // only when it really lives inside our own container.
        if (convertedPath.length && [oldAGX hasPrefix:convertedPath]) {
            [fm removeItemAtPath:oldAGX error:nil];
            NSString *ico = [oldAGX.stringByDeletingPathExtension
                             stringByAppendingPathExtension:@"ico"];
            [fm removeItemAtPath:ico error:nil];
        }

        NSLog(@"AGT migration: re-pointed \"%@\" from %@ to %@",
              game.metadata.title, oldAGX.lastPathComponent, dssURL.path);
    }

    [defaults setBool:YES forKey:@"AGTConvertedToDirectMigrationDone"];
}

@end

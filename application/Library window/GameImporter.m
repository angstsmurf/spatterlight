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
#import "Ifid.h"
#import "Image.h"
#import "Fetches.h"

#import "CoreDataManager.h"

#import "IFDBDownloader.h"
#import "CoverImageStore.h"

#import "NSString+Categories.h"
#import "NSData+Categories.h"
#import "FileSignature.h"
#import "Preferences.h"
#import "Constants.h"

// Treaty of babel header
#include "babel_handler.h"
#import "IFictionMetadata.h"
#import "IFStory.h"
#import "Metadata+IFiction.h"

#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "TableViewController+LibraryManagement.h"
#import "MetadataHandler.h"

#import "FolderAccess.h"
#import "NSManagedObjectContext+safeSave.h"
#import "ImageCompareViewController.h"
#import "LibraryOrganizer.h"
#import "NSAlert+Spatterlight.h"

extern NSArray *gGameFileTypes;

// Computes an AGT game's Treaty-of-Babel IFID directly from its .D$$/.DA1
// data files, in-process, without converting to AGX first. Implemented in
// terps/agility/agtifid.c (linked into the app). Returns a malloc'd string
// the caller frees, or NULL if the file set is not a readable AGT game.
extern char *agt_copy_ifid_for_file(const char *path);

@implementation GameImporter

// agt_copy_ifid_for_file() runs the AGT reader, which uses global state and is
// not thread-safe. Imports and the AGT migration can run on different
// background contexts, so funnel every call through one process-wide lock.
+ (nullable NSString *)identifyAGTFile:(NSString *)path {
    @synchronized ([GameImporter class]) {
        char *raw = agt_copy_ifid_for_file(path.fileSystemRepresentation);
        if (!raw)
            return nil;
        NSString *ifid = @(raw);
        free(raw);
        return ifid.length ? ifid : nil;
    }
}

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
    //Count in the import context (we are usually on its queue), not the
    //main one.
    NSManagedObjectContext *importContext = options[@"context"];
    NSFetchRequest *countRequest = [NSFetchRequest fetchRequestWithEntityName:@"Game"];
    countRequest.predicate = [NSPredicate predicateWithFormat:@"hidden == NO"];
    NSUInteger __block libraryCount = 0;
    [importContext performBlockAndWait:^{
        libraryCount = [importContext countForFetchRequest:countRequest error:NULL];
    }];
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
    NSManagedObjectContext *context = options[@"context"];

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
    }
    return lastOperation;
}

void ReportImportFailure(NSString *title, NSString *info) {
    void (^show)(void) = ^{
        [[NSAlert alertWithMessage:title info:info buttons:nil] runModal];
    };
    if (NSThread.isMainThread)
        show();
    else
        dispatch_async(dispatch_get_main_queue(), show);
}

+ (BabelIdentifyResult)identifyFile:(NSString *)path format:(NSString **)outFormat ifid:(NSString **)outIfid {
    if (outFormat)
        *outFormat = nil;
    if (outIfid)
        *outIfid = nil;
    if (!path.length)
        return kBabelUnknownFormat;

    void *ctx = get_babel_ctx();
    if (!ctx)
        return kBabelUnknownFormat;

    char pathBuf[PATH_MAX];
    strlcpy(pathBuf, path.fileSystemRepresentation, sizeof(pathBuf));
    char *format = babel_init_ctx(pathBuf, ctx);
    if (!format || !babel_get_authoritative_ctx(ctx)) {
        babel_release_ctx(ctx);
        free(ctx);
        return kBabelUnknownFormat;
    }

    // Skip any qualifier babel puts before the format name
    char *space = strchr(format, ' ');
    if (outFormat)
        *outFormat = @(space ? space + 1 : format);

    char buf[TREATY_MINIMUM_EXTENT];
    int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, ctx);
    babel_release_ctx(ctx);
    free(ctx);

    if (rv <= 0)
        return kBabelNoIfid;

    char *comma = strchr(buf, ',');
    if (comma)
        *comma = 0;
    if (!buf[0])
        return kBabelNoIfid;

    if (outIfid)
        *outIfid = @(buf);
    return kBabelIdentified;
}

//! Imports a file that is not a game: an iFiction file is read into the
//! library's metadata, anything else is reported as an unknown format.
- (void)importNonGameFile:(NSString *)path inContext:(NSManagedObjectContext *)context report:(BOOL)report {
    if ([path.pathExtension.lowercaseString isEqualToString:@"ifiction"]) {
        NSData *data = [NSData dataWithContentsOfFile:path];
        if (data) {
            NSDictionary<NSString *, IFStory *> *indirectMatches;
            NSDictionary<NSString *, IFStory *> *exactMatches = [MetadataHandler importMetadataFromXML:data indirectMatches:&indirectMatches inContext:context];
            [_tableViewController.ifictionMatches addEntriesFromDictionary:exactMatches];
            [_tableViewController.ifictionPartialMatches addEntriesFromDictionary:indirectMatches];
            return;
        }
    }

    if (report) {
        ReportImportFailure(NSLocalizedString(@"Unknown file format.", nil),
                            [NSString stringWithFormat:NSLocalizedString(@"Can not recognize the file extension \".%@\"", nil), path.pathExtension]);
    }
}

//! Asks babel for the format and IFID, reporting or logging why if it can't
//! give both.
- (BOOL)identifyWithBabel:(NSString *)path report:(BOOL)report format:(NSString * _Nullable * _Nonnull)formatStr ifid:(NSString * _Nullable * _Nonnull)ifid {
    switch ([GameImporter identifyFile:path format:formatStr ifid:ifid]) {
        case kBabelIdentified:
            return YES;
        case kBabelUnknownFormat:
            if (report)
                ReportImportFailure(NSLocalizedString(@"Unknown file format.", nil),
                                    NSLocalizedString(@"Babel can not identify the file format.", nil));
            return NO;
        case kBabelNoIfid:
            if (report)
                ReportImportFailure(NSLocalizedString(@"Fatal error.", nil),
                                    NSLocalizedString(@"Can not compute IFID from the file.", nil));
            else
                NSLog(@"Babel can not compute IFID from the file.");
            return NO;
    }
}

//! Whether we have an interpreter for the format, and the format is one we
//! expect behind the file extension.
- (BOOL)isSupportedFormat:(NSString *)formatStr extension:(NSString *)extension data:(nullable NSData *)fileData {
    NSString *terp = gExtMap[extension];

    if (!terp)
        terp = gFormatMap[formatStr];

    if (!terp)
        return NO;

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
    if (!allowedFormats)
        return YES;

    if (![allowedFormats containsObject:formatStr])
        return NO;

    // .dat + zcode additionally requires the file to actually look like Z-code,
    // since plenty of unrelated games ship a story file called e.g. "story.dat".
    if ([extension isEqualToString:@"dat"] && [formatStr isEqualToString:@"zcode"])
        return [GameImporter looksLikeZcode:fileData];

    return YES;
}

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide {
    NSString *ifid = nil;
    NSString *formatStr = nil;

    NSString *extension = path.pathExtension.lowercaseString;

    // AGT games are played directly from their original .D$$/.DA1 file set,
    // with no AGX conversion. We identify the game in-process and leave the
    // path pointing at the original .D$$ -- the agility terp loads the file
    // set in place (its siblings are reachable through the folder-level
    // sandbox access granted at launch). babel only understands AGX, so for
    // the direct path we skip the babel identification below.
    BOOL directAGT = NO;

    if ([extension isEqualToString: @"d$$"]) {
        ifid = [GameImporter identifyAGTFile:path];
        if (!ifid) {
            if (report) {
                ReportImportFailure(NSLocalizedString(@"Could not read game.", nil),
                                    NSLocalizedString(@"This file could not be read as an AGT game.", nil));
            } else {
                NSLog(@"This file could not be read as an AGT game.");
            }
            return nil;
        }
        formatStr = @"agt";
        directAGT = YES;
    }

    if (![gGameFileTypes containsObject:extension]) {
        [self importNonGameFile:path inContext:context report:report];
        return nil;
    }

    // The file is read once, mapped, and the data shared by every step below
    // (babel reads it on its own).
    NSData *fileData = [NSData dataWithContentsOfURL:[NSURL fileURLWithPath:path isDirectory:NO]
                                             options:NSDataReadingMappedAlways error:NULL];

    Blorb *blorb = nil;
    if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path isDirectory:NO]]) {
        blorb = [[Blorb alloc] initWithData:fileData];
        BlorbResource *executable = [blorb resourcesForUsage:ExecutableResource].firstObject;
        if (!executable) {
            if (report) {
                ReportImportFailure(NSLocalizedString(@"Not a game.", nil),
                                    NSLocalizedString(@"No executable chunk found in Blorb file.", nil));
            }
            return nil;
        }

        // ADRIFT games shipped in a Blorb (an ADRI executable chunk) are
        // identified by the normal babel pass below: babel/blorb.c maps the
        // ADRI chunk to the "adrift" format and reads the IFID/metadata from
        // the IFmd chunk, and the scarier terp plays the Blorb directly.
    }

    if (!directAGT && ![self identifyWithBabel:path report:report format:&formatStr ifid:&ifid])
        return nil;

    if (![self isSupportedFormat:formatStr extension:extension data:fileData]) {
        if (report) {
            ReportImportFailure(NSLocalizedString(@"Unknown file format.", nil),
                                NSLocalizedString(@"Not a supported format.", nil));
        }
        return nil;
    }

    if (!fileData)
        return nil;
    NSString *hash = [FileSignature signatureFromData:fileData];
    if (!hash.length)
        return nil;

    Game __block *game;
    [context performBlockAndWait:^{
        game = [self insertGameAtPath:path ifid:ifid format:formatStr hash:hash data:fileData blorb:blorb hide:hide inContext:context];
    }];

    return game;
}

//! Creates or updates the Game and Metadata objects of an identified game
//! file. Runs on the context's queue.
- (Game *)insertGameAtPath:(NSString *)path ifid:(NSString *)ifid format:(NSString *)formatStr hash:(NSString *)hash data:(NSData *)fileData blorb:(nullable Blorb *)blorb hide:(BOOL)hide inContext:(NSManagedObjectContext *)context {
    // We really should check if there is a game-less metadata object that we can use here
    // but we skip this for now.
    Metadata *metadata = [Fetches fetchMetadataForHash:hash inContext:context];
    Game *game = metadata.game;

    if (!game) {
        game = (Game *) [NSEntityDescription
                         insertNewObjectForEntityForName:@"Game"
                         inManagedObjectContext:context];
    }

    if (!metadata) {
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
    game.detectedFormat = formatStr;
    game.hashTag = hash;
    metadata.hashTag = hash;

    if (!metadata.cover && blorb) {
        NSData *imageData = blorb.coverImageData;
        if (imageData) {
            metadata.coverArtURL = path;
            [CoverImageStore insertImageData:imageData inMetadataID:metadata.objectID context:context];
        }
    }

    game.metadata = metadata;

    if ([game.detectedFormat isEqualToString:@"zcode"]) {
        // Used when associating save file with game
        [GameImporter addZCodeIDFromData:fileData blorb:blorb toGame:game];
    } else if ([game.detectedFormat isEqualToString:@"glulx"]) {
        // Used when associating save file with game
        game.serialString = [path oldSignatureFromFile];
    }

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

    return game;
}

+ (void)importInfoFromXML:(NSData *)mdbuf intoMetadata:(Metadata * _Nonnull)metadata {
    IFStory *story = [[IFictionMetadata alloc] initWithData:mdbuf].firstStory;
    if (story)
        [metadata addInfoFromStory:story];
}

- (void)updateImageFromBlorb:(Blorb *)blorb inGame:(Game *)game {
    if (blorb) {
        NSData *newImageData = blorb.coverImageData;
        kImageReplacementPrefsType userSetting = (kImageReplacementPrefsType)[[NSUserDefaults standardUserDefaults] integerForKey:kImageReplacementKey];
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
                        ImageCompareViewController *imageCompare = [ImageCompareViewController new];
                        if ([imageCompare userWantsImage:newImageData ratherThanImage:oldImageData source:kImageComparisonLocalFile force:NO]) {
                            [CoverImageStore insertImageData:newImageData inMetadataID:game.metadata.objectID context:game.managedObjectContext];
                        }
                    });
                } else {
                    [CoverImageStore insertImageData:newImageData inMetadataID:game.metadata.objectID context:game.managedObjectContext];
                }
            }
        }
    }
}

static inline uint16_t word(const uint8_t *memory, uint32_t addr)
{
    return (uint16_t)((memory[addr] << 8) | memory[addr + 1]);
}

//! Reads the release, serial and checksum of a Z-code game, from the Blorb
//! if it has them, otherwise from the story file header. @c blorb is the
//! Blorb read from @c data, or nil if the game is a bare story file.
+ (void)addZCodeIDFromData:(NSData *)data blorb:(nullable Blorb *)blorb toGame:(Game *)game {
    if (blorb) {
        if (blorb.checkSum && blorb.serialNumber && blorb.releaseNumber) {
            game.checksum = (int32_t)blorb.checkSum;
            game.serialString = blorb.serialNumber;
            game.releaseNumber = (int32_t)blorb.releaseNumber;
            return;
        }
        BlorbResource *zcode = [blorb resourcesForUsage:ExecutableResource].firstObject;
        // Without an executable chunk there is no Z-code header to read:
        // don't fall back to reading the IFF container as one.
        if (!zcode)
            return;
        data = [blorb dataForResource:zcode];
    }
    // The fields read below end at byte 30 of the Z-machine header.
    if (data.length < 30)
        return;
    const unsigned char *ptr = data.bytes;
    game.releaseNumber = word(ptr, 2);
    NSData *serialData = [NSData dataWithBytes:ptr + 18 length:6];
    game.serialString = [[NSString alloc] initWithData:serialData encoding:NSASCIIStringEncoding];
    game.checksum = word(ptr, 28);
}

+ (BOOL)looksLikeZcode:(nullable NSData *)mem {
    // A Z-code header is 64 bytes
    if (mem.length < 64)
        return NO;
    const uint8_t *memory = mem.bytes;

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

//! The image files that may be the cover of a game: in its folder, its
//! "Cover" subfolder, or the folder above a "Data" folder. Sets
//! @c screenDatCopy to a temporary copy of SCREEN.DAT that the caller deletes.
- (NSArray<NSURL *> *)candidateImageFilesForGame:(Game *)game screenDatCopy:(NSURL * _Nullable * _Nonnull)screenDatCopy {
    *screenDatCopy = nil;
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
            // in a temp folder, which is deleted below.
            *screenDatCopy = [self neoCopyOfScreenDatBeside:game.path];
            if (*screenDatCopy)
                imageFiles = @[*screenDatCopy];
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

    return imageFiles;
}

//! The image in a file, converting the retro formats and taking the cover of
//! a Blorb. nil or empty if there is none.
+ (nullable NSData *)imageDataFromURL:(NSURL *)url {
    NSString *imgExtension = url.pathExtension.lowercaseString;

    if ([imgExtension isEqualToString:@"mg1"])
        return [NSData imageDataFromMG1URL:url];
    if ([imgExtension isEqualToString:@"neo"])
        return [NSData imageDataFromNeoURL:url];

    if ([imgExtension isEqualToString:@"blb"] || [imgExtension isEqualToString:@"blorb"])
        return [Blorb coverImageDataAtURL:url];
    return [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:NULL];
}

- (void)lookForImagesForGame:(Game *)game {
    NSURL *screenDatCopy = nil;
    NSArray<NSURL *> *imageFiles = [self candidateImageFilesForGame:game screenDatCopy:&screenDatCopy];

    NSData *imageData = nil;
    NSURL *chosenURL = nil;
    for (NSURL *url in imageFiles) {
        imageData = [GameImporter imageDataFromURL:url];
        if (imageData.length) {
            chosenURL = url;
            break;
        }
    }

    if (screenDatCopy) {
        [[NSFileManager defaultManager] removeItemAtURL:screenDatCopy.URLByDeletingLastPathComponent error:NULL];
        if (chosenURL)
            chosenURL = [NSURL fileURLWithPath:[game.path.stringByDeletingLastPathComponent stringByAppendingPathComponent:@"SCREEN.DAT"] isDirectory:NO];
    }

    if (!chosenURL)
        return;

    game.metadata.coverArtURL = chosenURL.path;
    [CoverImageStore insertImageData:imageData inMetadataID:game.metadata.objectID context:game.managedObjectContext];
}


// Creates a renamed copy of SCREEN.DAT in a new temporary directory, so the
// cover image importing code recognizes it as a Neochrome image file. Returns
// nil if there is no SCREEN.DAT. The caller deletes the directory.
- (nullable NSURL *)neoCopyOfScreenDatBeside:(NSString *)path {
    NSFileManager *filemanager = [NSFileManager defaultManager];

    NSString *screenDatPath = [path.stringByDeletingLastPathComponent
                               stringByAppendingPathComponent:@"SCREEN.DAT"];
    if (![filemanager fileExistsAtPath:screenDatPath])
        return nil;

    NSError *error = nil;
    NSURL *temporaryDirectoryURL = [filemanager
                                    URLForDirectory:NSItemReplacementDirectory
                                    inDomain:NSUserDomainMask
                                    appropriateForURL:[NSURL fileURLWithPath:path isDirectory:NO]
                                    create:YES
                                    error:&error];
    if (!temporaryDirectoryURL) {
        NSLog(@"neoCopyOfScreenDatBeside: %@", error);
        return nil;
    }

    NSURL *newURL = [temporaryDirectoryURL URLByAppendingPathComponent:@"SCREEN.neo" isDirectory:NO];
    if (![filemanager copyItemAtURL:[NSURL fileURLWithPath:screenDatPath isDirectory:NO]
                              toURL:newURL
                              error:&error]) {
        NSLog(@"neoCopyOfScreenDatBeside: %@", error);
        [filemanager removeItemAtURL:temporaryDirectoryURL error:NULL];
        return nil;
    }
    return newURL;
}

@end

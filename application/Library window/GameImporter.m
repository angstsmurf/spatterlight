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

#import "CoreDataManager.h"

#import "IFDBDownloader.h"

#import "NSString+Categories.h"
#import "NSData+Categories.h"
#import "OSImageHashing.h"

// Treaty of babel header
#include "babel_handler.h"

#import "LibController.h"

#import "FolderAccess.h"
#import "NSManagedObjectContext+safeSave.h"
#import "ImageCompareViewController.h"
#import "ComparisonOperation.h"

extern NSArray *gGameFileTypes;

@implementation GameImporter

- (instancetype)initWithLibController:(LibController *)libController {
    self = [super init];
    if (self) {
        _libController = libController;
    }
    return self;
}

- (void)addFiles:(NSArray<NSURL *> *)urls options:(NSDictionary *)options {

    NSManagedObjectContext *context = options[@"context"];
    NSMutableArray *select = nil;

    //Don't select every game after import if we start with no games
    if (_libController.gameTableModel.count)
        select = [NSMutableArray arrayWithCapacity:urls.count];

    BOOL reportFailure = NO;

    if (urls.count == 1) {
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

    LibController *libController = _libController;

#pragma mark Long completion handler
    // A long block that will run when all files are added
    // and all metadata is downloaded
    void (^internalHandler)(void) = ^void() {

        if (!libController.currentlyAddingGames)
            return;

        libController.currentlyAddingGames = NO;

        [context safeSaveAndWait];
        [libController.coreDataManager saveChanges];

        [FolderAccess releaseBookmark:[FolderAccess suitableDirectoryForURL:urls.firstObject]];
        [context performBlock:^{

            dispatch_async(dispatch_get_main_queue(), ^{
                libController.addButton.enabled = YES;
                libController.currentlyAddingGames = NO;
                [libController endImporting];
                [libController selectGamesWithIfids:select scroll:YES];
            });
            if ([options[@"downloadInfo"] isEqual:@(YES)])
                [LibController fixMetadataWithNoIfidsInContext:context];
        }];
    };
    
    // End of the long block
#pragma mark End of long completion handler
    
    newOptions[@"completionHandler"] = internalHandler;
    
    [libController beginImporting];
    [self addGamesFromURLsRecursively:urls options:newOptions];
}


- (nullable NSOperation *)addGamesFromURLsRecursively:(NSArray*)urls options:(NSDictionary *)options
{
    _downloadedMetadata = [NSMutableSet new];
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

    for (NSURL *url in urls)
    {
        if (!_libController.currentlyAddingGames) {
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

        if ([timestamp timeIntervalSinceNow] < -0.3) {
            [context safeSaveAndWait];
            [_libController.coreDataManager saveChanges];
            timestamp = [NSDate date];
        }
    }

    NSBlockOperation *finisher = nil;
    if (internalHandler) {
        finisher = [NSBlockOperation blockOperationWithBlock:internalHandler];
        if (lastOperation)
            [finisher addDependency:lastOperation];
        finisher.qualityOfService = NSQualityOfServiceUtility;
        [_libController.downloadQueue addOperation:finisher];
        lastOperation = nil;
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

    NSOperation *lastOperation = nil;

    Game *game = [self importGame:url.path inContext:context reportFailure:reportFailure hide:NO];

    if (game) {
        [_libController beginImporting];
        if (select)
            [select addObject:game.ifid];
        if (downloadInfo && ![_downloadedMetadata containsObject:game.metadata]) {
            IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithContext:context];
            lastOperation = [downloader downloadMetadataForGames:@[game] onQueue:_libController.downloadQueue imageOnly:NO reportFailure:NO completionHandler:^{
                [game.managedObjectContext performBlock:^{
                    game.hasDownloaded = YES;
                }];
            }];
            [_downloadedMetadata addObject:game.metadata];
        }
        // We only look for images on the HDD if the game has
        // no cover image or the Inform 7 placeholder image.
        if (lookForImages && (game.metadata.cover.data == nil || [(NSData *)game.metadata.cover.data isPlaceHolderImage]))
            [self lookForImagesForGame:game];
    } else {
        //NSLog(@"libctl: addFile: File %@ not added!", url.path);
    }
    return lastOperation;
}

- (nullable Game *)importGame:(NSString*)path inContext:(NSManagedObjectContext *)context reportFailure:(BOOL)report hide:(BOOL)hide {
    char buf[TREATY_MINIMUM_EXTENT];
    Metadata __block *metadata;
    Game __block *game;
    Blorb __block *blorb = nil;
    NSString *ifid;
    char *format;
    char *s;
    int rv;

    NSString *extension = path.pathExtension.lowercaseString;

    if ([extension isEqualToString: @"d$$"]) {
        path = [self convertAGTFile:path];
        if (!path) {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Conversion failed.", nil);
                    alert.informativeText = NSLocalizedString(@"This old style AGT file could not be converted to the new AGX format.", nil);
                    [alert runModal];
                });
            } else {
                NSLog(@"This old style AGT file could not be converted to the new AGX format.");
            }
            return nil;
        }
    }

    if (![gGameFileTypes containsObject:extension])
    {
        if (report) {
            if ([extension isEqualToString:@"ifiction"]) {
                [_libController waitToReportMetadataImport];
                [_libController importMetadataFromFile:path inContext:context];
                return nil;
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Can not recognize the file extension \".%@\"", nil), path.pathExtension];
                [alert runModal];
            });
        }
        return nil;
    }

    if ([extension isEqualToString:@"blorb"] || [extension isEqualToString:@"blb"]) {
        blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];
        BlorbResource *executable = [blorb resourcesForUsage:ExecutableResource].firstObject;
        if (!executable) {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Not a game.", nil);
                    alert.informativeText = NSLocalizedString(@"No executable chunk found in Blorb file.", nil);
                    [alert runModal];
                });
            } else {
                NSLog(@"No executable chunk found in Blorb file.");
            }
            return nil;
        }

        if ([executable.chunkType isEqualToString:@"ADRI"]) {
            if (report) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = NSLocalizedString(@"Unsupported format.", nil);
                    alert.informativeText = NSLocalizedString(@"Adrift 5 games are not supported.", nil);
                    [alert runModal];
                });
            } else {
                NSLog(@"Adrift 5 games are not supported.");
            }
            return nil;
        }
    }

    void *ctx = get_babel_ctx();
    format = babel_init_ctx((char*)path.UTF8String, ctx);
    if (!format || !babel_get_authoritative_ctx(ctx))
    {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = NSLocalizedString(@"Babel can not identify the file format.", nil);
                [alert runModal];
            });
        }
        babel_release_ctx(ctx);
        return nil;
    }

    s = strchr(format, ' ');
    if (s) format = s+1;

    rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, ctx);
    if (rv <= 0)
    {
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
        babel_release_ctx(ctx);
        return nil;
    }

    s = strchr(buf, ',');
    if (s) *s = 0;
    ifid = @(buf);

    if ([ifid isEqualToString:@"ZCODE-5-------"] && [[path signatureFromFile] isEqualToString:@"0304000545ff60e931b802ea1e6026860000c4cacbd2c1cb022acde526d400000000000000000000000000000000000000000000000000000000000000000000"])
        ifid = @"ZCODE-5-830222";

    if (([extension isEqualToString:@"dat"] &&
         !(([@(format) isEqualToString:@"zcode"] && [self checkZcode:path]) ||
           [@(format) isEqualToString:@"level9"] ||
           [@(format) isEqualToString:@"advsys"] ||
           [@(format) isEqualToString:@"sagaplus"] ||
           [@(format) isEqualToString:@"scott"] )) ||
        ([extension isEqualToString:@"gam"] && ![@(format) isEqualToString:@"tads2"]) ||
        (([extension isEqualToString:@"d64"] || [extension isEqualToString:@"dsk"]) && ![@(format) isEqualToString:@"scott"] && ![@(format) isEqualToString:@"taylor"])) {
        if (report) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = NSLocalizedString(@"Unknown file format.", nil);
                alert.informativeText = NSLocalizedString(@"Not a supported format.", nil);
                [alert runModal];
            });
        } else {
            NSLog(@"%@: Recognized extension (%@) but unknown file format.", [path lastPathComponent], extension);
        }
        babel_release_ctx(ctx);
        return nil;
    }

    //NSLog(@"libctl: import game %@ (%s)", path, format);

    if (ifid == nil) {
        // If this happens, it means the Babel tool did not
        // work as it should. It detected the game but returned
        // an invalid IFID.
        NSLog(@"Error! Ifid nil! buf:%s (%x%x%x%x)", buf, buf[0], buf[1], buf[2], buf[3]);
        return nil;
    }

    LibController *libController = _libController;

    [context performBlockAndWait:^{

        metadata = [LibController fetchMetadataForIFID:ifid inContext:context];

        if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path]] && !blorb)
            blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:path]];

        if (!metadata)
        {
            if (blorb) {
                NSData *mdbufData = [blorb metaData];
                if (mdbufData) {
                    metadata = [libController importMetadataFromXML:mdbufData inContext:context];
                    metadata.source = @(kInternal);
                    NSLog(@"Extracted metadata from blorb. Title: %@", metadata.title);
                }
                else NSLog(@"Found no metadata in blorb file %@", path);
            }
        } else {
            game = [LibController fetchGameForIFID:ifid inContext:context];
            if (game) {
                if ([game.detectedFormat isEqualToString:@"glulx"])
                    game.hashTag = [path signatureFromFile];
                else if ([game.detectedFormat isEqualToString:@"zcode"]) {
                    [self addZCodeIDfromFile:path blorb:blorb toGame:game];
                }
                if (![path isEqualToString:game.path])
                {
                    NSLog(@"File location did not match for %@. Updating library with new file location (%@).", [path lastPathComponent], path);
                    [game bookmarkForPath:path];
                }
                if (![game.detectedFormat isEqualToString:@(format)]) {
                    NSLog(@"Game format did not match for %@. Updating library with new detected format (%s).", [path lastPathComponent], format);
                    game.detectedFormat = @(format);
                }
                if (blorb) {
                    [self updateImageFromBlorb:blorb inGame:game];
                }
                game.found = YES;
                if (!hide)
                    game.hidden = NO;
                return;
            }
        }

        if (!metadata)
        {
            metadata = (Metadata *) [NSEntityDescription
                                     insertNewObjectForEntityForName:@"Metadata"
                                     inManagedObjectContext:context];
        }

        [metadata findOrCreateIfid:ifid];

        if (!metadata.format)
            metadata.format = @(format);
        if (!metadata.title || metadata.title.length == 0)
        {
            metadata.title = path.lastPathComponent;
        }

        if (!metadata.cover)
        {
            NSURL *imgURL = [NSURL URLWithString:[ifid stringByAppendingPathExtension:@"tiff"] relativeToURL:libController.imageDir];
            NSData *img = [[NSData alloc] initWithContentsOfURL:imgURL];
            if (img)
            {
                NSLog(@"Found cover image in image directory for game %@", metadata.title);
                metadata.coverArtURL = imgURL.path;
                [IFDBDownloader insertImageData:img inMetadata:metadata];
            }
            else
            {
                if (blorb) {
                    NSData *imageData = blorb.coverImageData;
                    if (imageData) {
                        metadata.coverArtURL = path;
                        [IFDBDownloader insertImageData:imageData inMetadata:metadata];
                        NSLog(@"Extracted cover image from blorb for game %@", metadata.title);
                        NSLog(@"Image md5: %@", [imageData md5String]);
                        // 26BFA026324DC9C5B3080EA9769B29DE
                    }
                    else NSLog(@"Found no image in blorb file %@", path);
                }
            }
        }

        babel_release_ctx(ctx);

        game = (Game *) [NSEntityDescription
                         insertNewObjectForEntityForName:@"Game"
                         inManagedObjectContext:context];

        [game bookmarkForPath:path];

        game.added = [NSDate date];
        game.hidden = hide;
        game.metadata = metadata;
        game.ifid = ifid;
        game.detectedFormat = @(format);
        if ([game.detectedFormat isEqualToString:@"glulx"]) {
            game.hashTag = [path signatureFromFile];
        }
        if ([game.detectedFormat isEqualToString:@"zcode"]) {
            [self addZCodeIDfromFile:path blorb:blorb toGame:game];
        }
    }];

    return game;
}

- (void)updateImageFromBlorb:(Blorb *)blorb inGame:(Game *)game {
    if (blorb) {
        NSData *newImageData = blorb.coverImageData;
        if (newImageData && ![newImageData isPlaceHolderImage] && ![_libController.lastImageComparisonData isEqual:newImageData]) {
            _libController.lastImageComparisonData = newImageData;
            NSData *oldImageData = (NSData *)game.metadata.cover.data;
            kImageComparisonResult comparisonResult = [ImageCompareViewController chooseImageA:newImageData orB:oldImageData source:kImageComparisonDownloaded force:NO];

            if (comparisonResult != kImageComparisonResultA && comparisonResult == kImageComparisonResultWantsUserInput) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    ImageCompareViewController *imageCompare = [[ImageCompareViewController alloc] initWithNibName:@"ImageCompareViewController" bundle:nil];
                    if ([imageCompare userWantsImage:newImageData ratherThanImage:oldImageData source:kImageComparisonLocalFile force:NO]) {
                        [IFDBDownloader insertImageData:newImageData inMetadata:game.metadata];
                    }
                });
            } else {
                [IFDBDownloader insertImageData:newImageData inMetadata:game.metadata];
            }
        }
    }
}

- (void)addZCodeIDfromFile:(NSString *)path blorb:(nullable Blorb *)blorb toGame:(Game *)game {
    BOOL found = NO;
    NSData *data = nil;
    if ([Blorb isBlorbURL:[NSURL fileURLWithPath:path]]) {
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

    int dictionary = word(memory, 0x08);
    int static_start = word(memory, 0x0e);

    if(dictionary != 0 && dictionary < static_start)
    {   // corrupted story: dictionary is not in static memory
        return NO;
    }

    int objects = word(memory, 0x0a);
    int zversion = memory[0x00];
    int propsize = (zversion <= 3 ? 62UL : 126UL);

    if(objects < 64 ||
       objects + propsize > static_start)
    {
        // corrupted story: object table is not in dynamic memory
        return NO;
    }
    return YES;
}

- (void)lookForImagesForGame:(Game *)game {
    //    NSLog(@"lookForImagesForGame %@", game.metadata.title);
    NSFileManager *filemgr = [NSFileManager defaultManager];
    NSURL *dirUrl = [NSURL fileURLWithPath:[game.path stringByDeletingLastPathComponent] isDirectory:YES];
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
        [IFDBDownloader insertImageData:imageData inMetadata:game.metadata];
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

    NSString *tempFilePath = [temporaryDirectoryURL.path stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];

    NSString *screenDatPath = [path.stringByDeletingLastPathComponent
                               stringByAppendingPathComponent:@"SCREEN.DAT"];

    if ([filemanager fileExistsAtPath:screenDatPath]) {
        NSURL *oldURL = [NSURL fileURLWithPath:screenDatPath];
        NSString *newURLpath = [tempFilePath stringByAppendingPathExtension:@"neo"];
        NSURL *newURL =  [NSURL fileURLWithPath:newURLpath];

        [filemanager
         copyItemAtURL:oldURL
         toURL:newURL
         error:&error];
        return @[newURL];
    }
    return @[];
}

// Converts AGT D$$ files to the AGX format
// that AGiliTy can play. The resulting file is kept in
// a special Application Support directory.
// Also copies any icon files to the same directory
// for the cover image importer to find.

- (NSString *)convertAGTFile:(NSString *)origpath {

    NSError *error = nil;

    NSFileManager *filemanager = [NSFileManager defaultManager];


    NSURL *desktopURL = [NSURL fileURLWithPath:origpath
                                   isDirectory:YES];


    NSURL *temporaryDirectoryURL = [filemanager
                                    URLForDirectory:NSItemReplacementDirectory
                                    inDomain:NSUserDomainMask
                                    appropriateForURL:desktopURL
                                    create:YES
                                    error:&error];

    NSString *tempFilePath = [temporaryDirectoryURL.path stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];

    tempFilePath = [tempFilePath stringByAppendingPathExtension:@"agx"];

    NSString *exepath =
    [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"agt2agx"];
    if (!exepath.length)
        return nil;
    NSURL *dirURL;
    NSURL *cvtURL;

    NSTask *task;
    char *format;
    int status;

    task = [[NSTask alloc] init];
    task.launchPath = exepath;
    task.arguments = @[ @"-o", tempFilePath, origpath ];
    [task launch];
    [task waitUntilExit];
    status = task.terminationStatus;

    if (status != 0) {
        NSLog(@"GameImporter: agt2agx failed");
        return nil;
    }

    void *ctx = get_babel_ctx();

    format = babel_init_ctx((char *)tempFilePath.fileSystemRepresentation, ctx);
    if (!strcmp(format, "agt")) {
        char buf[TREATY_MINIMUM_EXTENT];
        int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, ctx);
        if (rv == 1) {
            dirURL =
            [_libController.homepath URLByAppendingPathComponent:@"Converted"];

            [filemanager createDirectoryAtURL:dirURL
                  withIntermediateDirectories:YES
                                   attributes:nil
                                        error:&error];

            cvtURL =
            [dirURL URLByAppendingPathComponent:
             [@(buf) stringByAppendingPathExtension:@"agx"]];

            babel_release_ctx(ctx);

            [filemanager removeItemAtURL:cvtURL error:nil];

            NSURL *tmp = [NSURL fileURLWithPath:tempFilePath];

            error = nil;
            status = [filemanager moveItemAtURL:tmp toURL:cvtURL error:&error];

            if (!status) {
                NSLog(@"GameImporter: could not move converted file: %@", error);
                return nil;
            }

            // We copy any icon files as well, so that the cover image importer
            // finds them.
            NSString *iconPath = [origpath.stringByDeletingPathExtension
                                  stringByAppendingPathExtension:@"ICO"];
            NSString *icon2Path =
            [[origpath.stringByDeletingPathExtension
              stringByAppendingString:@"2"] stringByAppendingPathExtension:@"ICO"];

            if ([filemanager fileExistsAtPath:icon2Path]) {
                iconPath = icon2Path;
            }

            if ([filemanager fileExistsAtPath:iconPath]) {
                NSLog(@"Found icon file at: %@", iconPath);
                NSURL *oldIconURL = [NSURL fileURLWithPath:iconPath];
                NSString *newIconPath = [cvtURL.path.stringByDeletingPathExtension stringByAppendingPathExtension:@"ico"];
                NSURL *newIconURL = [NSURL fileURLWithPath:newIconPath];

                [filemanager removeItemAtURL:newIconURL error:nil];

                [filemanager
                 copyItemAtURL:oldIconURL
                 toURL:newIconURL
                 error:&error];
            }

            return cvtURL.path;
        }
    } else {
        NSLog(@"GameImporter: babel did not like the converted file");
    }
    babel_release_ctx(ctx);
    return nil;
}

@end

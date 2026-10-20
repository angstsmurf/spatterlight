//
//  ImageView.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <QuartzCore/QuartzCore.h>
#import <BlorbFramework/BlorbFramework.h>

#import "ImageView.h"

#import "AppDelegate.h"
#import "TableViewController.h"
#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#import "NSImage+Categories.h"
#import "NSData+Categories.h"
#import "NSView+Categories.h"
#import "ImageCompareViewController.h"
#import "IFDBDownloader.h"
#import "CoverImageStore.h"
#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFCoverDescription.h"
#import "ImageDragSource.h"

#import "NotificationBezel.h"
#import "FolderAccess.h"

#import "ImageSimilarity.h"

#import "Preferences.h"
#import "NSAlert+Spatterlight.h"
#import "Constants.h"

#define FILTERTAG ((NSInteger) 100)
#define DESCRIPTIONTAG ((NSInteger) 200)
#define MAX_SPATTERLIGHT_IMAGE_FILE_SIZE ((NSInteger) 200000000)


@interface ImageView ()
{
    NSArray<NSString *> *imageTypes;
    CALayer *imageLayer;
    CAShapeLayer *shapeLayer;
    ImageDragSource *dragSource; // made on first drag
}

@property BOOL isSelected;
@property BOOL isReceivingDrag;
@property NSInteger numberForSelfSourcedDrag;
@property (readwrite) CGFloat ratio;
@property (readonly, copy, nullable) NSData *pngData;

@property NSOperationQueue *workQueue;
@property NSURL *destinationURL;

@end


@implementation ImageView

// This is called when loaded from InfoPanel.nib
- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        self.enabled = YES;

        [self registerForDraggedTypes:ImageView.acceptableDragTypes.allObjects];
        _numberForSelfSourcedDrag = NSNotFound;

        imageTypes = NSImage.imageTypes;
        imageTypes = [imageTypes arrayByAddingObjectsFromArray:@[ @"public.neochrome", @"public.mcga", @"public.dat", @"public.blorb" ]];
    }
    return self;
}

- (instancetype)initWithGame:(Game *)game image:(nullable NSImage *)anImage {
    if (!anImage && game.metadata.cover.data)
        anImage = [[NSImage alloc] initWithData:(NSData *)game.metadata.cover.data];
    if (anImage)
        self = [self initWithFrame:NSMakeRect(0, 0, anImage.size.width, anImage.size.height)];
    else
        self = [self initWithFrame:NSZeroRect];
    if (self) {
        _image = anImage;
        _gameObjID = game.objectID;

        if (_image)
            [self processImage:_image];
    }
    return self;
}

+ (NSSet<NSPasteboardType> *)acceptableDragTypes {
    static NSSet<NSPasteboardType> *types;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        types = [NSSet setWithObjects:NSPasteboardTypeURL, NSPasteboardTypeTIFF, NSPasteboardTypePNG, nil];
    });
    return types;
}

+ (NSMenu *)defaultMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    
    NSMenuItem *reload = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Reload from Blorb" , nil) action:@selector(reloadFromBlorb:) keyEquivalent:@""];
    NSMenuItem *open = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Select Image File…", nil) action:@selector(selectImageFile:) keyEquivalent:@""];
    NSMenuItem *filter = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Filter", nil) action:@selector(toggleFilter:) keyEquivalent:@""];
    NSMenuItem *download = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Download Image", nil) action:@selector(downloadImage:) keyEquivalent:@""];
    NSMenuItem *cut = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Cut", nil) action:@selector(cut:) keyEquivalent:@""];
    NSMenuItem *copy = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Copy", nil) action:@selector(copy:) keyEquivalent:@""];
    NSMenuItem *paste = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Paste", nil) action:@selector(paste:) keyEquivalent:@""];
    NSMenuItem *delete = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Delete", nil) action:@selector(delete:) keyEquivalent:@""];
    NSMenuItem *save = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Save Image As…", nil) action:@selector(saveImage:) keyEquivalent:@""];
    NSMenuItem *addDescription = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Add Description…", nil) action:@selector(addDescription:) keyEquivalent:@""];

    [menu addItem:open];
    [menu addItem:download];
    [menu addItem:reload];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:save];
    [menu addItem:[NSMenuItem separatorItem]];
    addDescription.tag = DESCRIPTIONTAG;
    [menu addItem:addDescription];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:cut];
    [menu addItem:copy];
    [menu addItem:paste];
    [menu addItem:delete];
    [menu addItem:[NSMenuItem separatorItem]];
    filter.state = NSControlStateValueOff;
    filter.tag = FILTERTAG;
    [menu addItem:filter];

    return menu;
}

- (void)updateLayer {
    if (shapeLayer) {
        // We delete the focus border if we are not selected or
        // it is the wrong size
        if ((!_isSelected && !_isReceivingDrag) ||
            !NSEqualRects(shapeLayer.frame, self.bounds)) {
            [CATransaction begin];
            [CATransaction setValue:(id)kCFBooleanTrue
                             forKey:kCATransactionDisableActions];
            [shapeLayer removeFromSuperlayer];
            shapeLayer = nil;
            self.layer.mask = nil;
            [CATransaction commit];
        }
    }

    if (_isSelected || _isReceivingDrag) {
        if (shapeLayer && NSEqualRects(shapeLayer.frame, self.bounds))
            return;
        if (self.frame.size.width * self.frame.size.height == 0)
            return;

        // Create a selection border
        shapeLayer = [CAShapeLayer layer];

        shapeLayer.fillColor = NSColor.clearColor.CGColor;
        shapeLayer.frame = self.bounds;

        shapeLayer.lineJoin = kCALineJoinRound;
        shapeLayer.strokeColor = NSColor.selectedControlColor.CGColor;
        CGFloat lineWidth = 4;
        shapeLayer.lineWidth = lineWidth;
        CGRect borderRect = NSMakeRect(lineWidth / 2, lineWidth / 2, self.bounds.size.width - lineWidth, self.bounds.size.height - lineWidth);
        CGPathRef roundedRectPath = CGPathCreateWithRoundedRect(borderRect, 2.5, 2.5, NULL);
        shapeLayer.path = roundedRectPath;
        shapeLayer.drawsAsynchronously = YES;
        [self.layer addSublayer:shapeLayer];
        CFRelease(roundedRectPath);

        // Use a mask layer to hide the sharp corners of the image
        // that stick out from the rounded corners of the selection border
        CAShapeLayer *masklayer = [CAShapeLayer layer];
        masklayer.fillColor = NSColor.blackColor.CGColor;
        masklayer.frame = self.bounds;
        masklayer.lineJoin = kCALineJoinRound;
        masklayer.lineWidth = 0;
        borderRect = NSMakeRect(0, 0, masklayer.frame.size.width,  masklayer.frame.size.height);
        roundedRectPath = CGPathCreateWithRoundedRect(borderRect, 5, 5, NULL);
        masklayer.path = roundedRectPath;
        self.layer.mask = masklayer;
        CFRelease(roundedRectPath);
    }
}


- (BOOL)wantsUpdateLayer {
    return YES;
}

- (Game *)gameFromObjectID {
    TableViewController *tableViewController = ((AppDelegate*)NSApp.delegate).tableViewController;
    return [tableViewController.managedObjectContext objectWithID:_gameObjID];
}

- (void)processImage:(NSImage *)image {
    _image = image;
    if (imageLayer)
        [imageLayer removeFromSuperlayer];

    imageLayer = [CALayer layer];

    NSImageRep *rep = image.representations.lastObject;
    NSSize sizeInPixels = NSMakeSize((CGFloat)rep.pixelsWide, (CGFloat)rep.pixelsHigh);
    image.size = sizeInPixels;

    _ratio = sizeInPixels.width / sizeInPixels.height;

    Game *game = [self gameFromObjectID];

    if (game.metadata.cover.interpolation == kUnset) {
        game.metadata.cover.interpolation = sizeInPixels.width < 350 ? kNearestNeighbor : kTrilinear;
    }

    imageLayer.magnificationFilter = (game.metadata.cover.interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;

    imageLayer.drawsAsynchronously = YES;
    imageLayer.contentsGravity = kCAGravityResize;

    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawOnSetNeedsDisplay;

    [CATransaction begin];
    [CATransaction setValue:(id)kCFBooleanTrue
                     forKey:kCATransactionDisableActions];

    imageLayer.contents = image;
    imageLayer.frame = self.bounds;
    imageLayer.bounds = self.bounds;

    [self.layer addSublayer:imageLayer];
    // Put any focus border on top
    if (shapeLayer) {
        [shapeLayer removeFromSuperlayer];
        [self.layer addSublayer:shapeLayer];
    }

    imageLayer.layoutManager  = [CAConstraintLayoutManager layoutManager];
    imageLayer.autoresizingMask = kCALayerHeightSizable | kCALayerWidthSizable;

    [CATransaction commit];

    self.accessibilityLabel = game.metadata.coverArtDescription;
    if (!self.accessibilityLabel.length)
        self.accessibilityLabel =
        [NSString stringWithFormat:@"cover image for %@", game.metadata.title];
}

- (BOOL)acceptsFirstResponder {
   return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    _isSelected = YES;
    [self updateLayer];
    return YES;
}

- (BOOL)resignFirstResponder {
    _isSelected = NO;
    [self updateLayer];
    return YES;
}

- (NSSize) intrinsicContentSize {
    if (@available(macOS 10.14, *)) {
        return [super intrinsicContentSize];
    } else {
        return _intrinsic;
    }
}

- (void)layout {
    [super layout];
    [self positionImagelayer];
}

- (void)positionImagelayer {
    if (self.ratio == 0 || !imageLayer) {
        return;
    }

    NSSize superSize = self.frame.size;

    NSRect imageFrame = NSMakeRect(0,0, superSize.width, superSize.width / self.ratio);
    if (NSMaxY(imageFrame) > NSMaxY(self.frame))
        imageFrame.origin.y = NSMaxY(self.frame) - imageFrame.size.height;

    [CATransaction begin];
    [CATransaction setValue:(id)kCFBooleanTrue
                     forKey:kCATransactionDisableActions];
    imageLayer.frame = imageFrame;
    if (shapeLayer) {
        [self updateLayer];
    }

    [CATransaction commit];
}

#pragma mark Actions

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    Game *game = [self gameFromObjectID];
    if (menuItem.action == @selector(paste:)) {
        NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
        if ([pasteBoard canReadObjectForClasses:@[[NSURL class]] options:@{NSPasteboardURLReadingContentsConformToTypesKey:[NSImage imageTypes]}]) {
            return YES;
        } else {
            NSMutableSet *types = [NSMutableSet setWithArray:pasteBoard.types];
            [types intersectSet:ImageView.acceptableDragTypes];
            if (types.count)
                return YES;
        }
        return NO;
    }
    
    else if (menuItem.action == @selector(cut:) || menuItem.action == @selector(copy:) || menuItem.action == @selector(delete:)) {
        return !_isPlaceholder;
    }

    else if (menuItem.action == @selector(reloadFromBlorb:)) {
        return [Blorb isBlorbURL:[NSURL fileURLWithPath:game.path isDirectory:NO]];
    }

    else if (menuItem.action == @selector(saveImage:)) {
        return (!_isPlaceholder);
    }

    else if (menuItem.action == @selector(toggleFilter:)) {
        NSMenuItem *filter = [self.menu itemWithTag:FILTERTAG];
        if (!_isPlaceholder) {
            filter.state = (game.metadata.cover.interpolation == kNearestNeighbor) ? NSControlStateValueOff : NSControlStateValueOn;
            return YES;
        } else {
            filter.state = NSControlStateValueOff;
            return NO;
        }
    }

    else if (menuItem.action == @selector(addDescription:)) {
        NSMenuItem *description = [self.menu itemWithTag:DESCRIPTIONTAG];
        if (!_isPlaceholder) {
            description.title = (game.metadata.coverArtDescription.length || game.metadata.cover.imageDescription.length) ? NSLocalizedString(@"Edit description", nil) : NSLocalizedString(@"Add description", nil);
            return YES;
        } else {
            description.title = NSLocalizedString(@"Add description", nil);
            return NO;
        }
    }

    return YES;
}

// Delete the cover relation of the Metadata object, and the Image object
// too if that leaves it an orphan
- (void)removeCover {
    Metadata *metadata = [self gameFromObjectID].metadata;
    Image *image = metadata.cover;
    if (!image)
        return;
    metadata.cover = nil;
    metadata.coverArtDescription = nil;
    [Image deleteIfOrphan:image];
}

- (void)cut:(id)sender {
    [self copy:nil];
    [self removeCover];
}

- (void)copy:(id)sender {
    if (!_image) {
        NSBeep();
        return;
    }
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb writeObjects:@[_image]];
}

- (void)delete:(id)sender {
    if (_isPlaceholder) {
        NSBeep();
        return;
    }
    NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"Are you sure?", nil)
                                          info:NSLocalizedString(@"Do you want to delete this cover image?", nil)
                                       buttons:@[NSLocalizedString(@"Delete", nil), NSLocalizedString(@"Cancel", nil)]];
    alert.icon = _image;

    if ([alert runModal] == NSAlertFirstButtonReturn)
        [self removeCover];
}

- (void)paste:(id)sender {
    NSPasteboard *pb = [NSPasteboard  generalPasteboard];
    NSArray *objects = [pb readObjectsForClasses:@[[NSURL class], [NSImage class]] options:nil];

    Game *game = [self gameFromObjectID];
    // The cover is stored as PNG or JPEG data, so it can't be compared with
    // a pasted NSImage byte for byte: compare both decoded to TIFF instead.
    NSData *coverData = (NSData *)game.metadata.cover.data;
    NSData *imgData1 = coverData ? [[NSImage alloc] initWithData:coverData].TIFFRepresentation : nil;

    BOOL (^isCurrentCover)(NSImage *) = ^BOOL(NSImage *image) {
        return imgData1 && [imgData1 isEqualToData:image.TIFFRepresentation];
    };

    for (id item in objects) {
        if ([item isKindOfClass:[NSImage class]]) {
            if (isCurrentCover((NSImage *)item)) {
                NSBeep();
                return;
            }
            [self replaceCoverImage:(NSImage *)item sourceUrl:@"pasteboard"];
            break;
        } else if ([item isKindOfClass:[NSURL class]]) {
            NSURL *url = (NSURL *)item;
            if ([game.metadata.coverArtURL isEqualToString:url.path]) {
                NSBeep();
                return;
            }
            if (![url.scheme isEqualToString:@"file"]) {
                NSArray *objects2 = [pb readObjectsForClasses:@[[NSImage class]] options:nil];
                if ([objects2.firstObject isKindOfClass:[NSImage class]]) {
                    NSImage *image = (NSImage *)objects2.firstObject;
                    if (isCurrentCover(image)) {
                        NSBeep();
                        return;
                    }
                    [self replaceCoverImage:image sourceUrl:url.path];
                    break;
                }
            }
            if ([self imageFromURL:url dontAsk:YES])
                break;
        }
    }
}

- (void)keyDown:(NSEvent *)event {
    NSString *characters = event.charactersIgnoringModifiers;
    unichar key = '\0';
    if (characters.length)
        key = [characters characterAtIndex:0];
    if (!_isPlaceholder && (key == NSDeleteCharacter || key == NSBackspaceCharacter))
        [self delete:nil];
    else
        [super keyDown:event];
}

- (IBAction)selectImageFile:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories = NO;
    panel.prompt = NSLocalizedString(@"Open", nil);

    panel.allowedFileTypes = imageTypes;

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        if (result == NSModalResponseOK) {
            double delayInSeconds = 0.3;
            dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
            dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
                [self imageFromURL:panel.URL dontAsk:YES];
            });
        }
    }];
}

- (IBAction)reloadFromBlorb:(id)sender {
    Game *game = [self gameFromObjectID];
    NSURL *url = game.urlForBookmark;
    if (!url)
        return;
    ImageView __weak *weakSelf = self;
    [FolderAccess askForAccessToURL:url andThenRunBlock:^{
        ImageView *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        Blorb *blorb = [Blorb blorbAtURL:url];
        NSData *data = blorb.coverImageData;
        BOOL success = NO;
        if (data) {
            [strongSelf processImageData:data sourceUrl:url.path dontAsk:YES];
            success = YES;
            NSData *metadata = blorb.metaData;
            if (metadata) {
                NSString *imageDescription = [ImageView coverArtDescriptionFromXMLData:metadata];
                if (imageDescription.length)
                    game.metadata.coverArtDescription = imageDescription;
            }
        }
        if (!success) {
            NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:strongSelf.window.screen];
            [bezel showStandardWithText:@"? No image found"];
        }
    }];
}

+ (NSString *)coverArtDescriptionFromXMLData:(NSData *)data {
    NSString *result = [[IFictionMetadata alloc] initWithData:data].firstStory.coverDescription.coverArtDescription;
    if (!result) {
        NSLog(@"No cover image description found in metadata!");
        return @"";
    }
    return result;
}

- (IBAction)downloadImage:(id)sender {
    TableViewController *libController = ((AppDelegate *)NSApp.delegate).tableViewController;
    libController.lastImageComparisonData = nil;
    NSManagedObjectContext *context = libController.managedObjectContext;
    Game *game = [context objectWithID:_gameObjID];
    NSOperationQueue *queue = self.workQueue;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger setting = [defaults integerForKey:kImageReplacementKey];

    BOOL reportFailure = (game.metadata.ifids.count <= 1 || game.metadata.tuid.length);

    NSArray<NSManagedObjectID *> *gameArray = @[game.objectID];
    NSManagedObjectID *metaID = game.metadata.objectID;
    NSString *oldURL = game.metadata.coverArtURL;
    [context performBlock:^{

        IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithTableViewController:libController];

        // If the metadata has more than one IFID, it may try to download metadata several times,
        // which may result in a failure bezel on one attempt while another one succeeds. Thus, if
        // the metadata has more than one IFID and no TUID, we dowload the image again
        // in order to get a failure report.
        // Also, if the image replacement setting is set to never replace, we override it by
        // downloading the image again with forceDialog set to YES.
        [downloader downloadMetadataForGames:gameArray inContext:context onQueue:queue imageOnly:YES reportFailure:reportFailure completionHandler:^{
            if (reportFailure && setting != kNeverReplace)
                return;
            BOOL __block reload = NO;
            [context performBlockAndWait:^{
                Metadata *meta = [context objectWithID:metaID];
                if ([meta.coverArtURL isEqualToString:oldURL]) {
                    reload = YES;
                } else if (setting == kNeverReplace) {
                    reload = YES;
                }
            }];

            if (reload)
                [downloader downloadImageFor:metaID inContext:context onQueue:queue forceDialog:(setting == kNeverReplace) reportFailure:YES];
        }];
    }];
}

- (IBAction)toggleFilter:(id)sender {
    Game *game = [self gameFromObjectID];
    game.metadata.cover.interpolation = (game.metadata.cover.interpolation == kTrilinear) ? kNearestNeighbor : kTrilinear;
    imageLayer.magnificationFilter = (game.metadata.cover.interpolation == kNearestNeighbor) ? kCAFilterNearest : kCAFilterTrilinear;
}

- (IBAction)saveImage:(id)sender {
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldLabel = NSLocalizedString(@"Save image: ", nil);
    panel.allowedFileTypes = @[ @"png" ];
    panel.extensionHidden = NO;
    [panel setCanCreateDirectories:YES];
    Game *game = [self gameFromObjectID];
    NSString *fileName = [game.path.lastPathComponent.stringByDeletingPathExtension stringByAppendingPathExtension:@"png"];
    if (!fileName.length)
        fileName = @"image.png";
    panel.nameFieldStringValue = NSLocalizedString(fileName, nil);

    [panel beginSheetModalForWindow:self.window
                  completionHandler:^(NSInteger result) {
        if (result == NSModalResponseOK) {
            NSURL *url = panel.URL;

            NSData *bitmapData = self.pngData;

            NSError *error = nil;

            if (![bitmapData writeToURL:url options:NSDataWritingAtomic error:&error]) {
                NSLog(@"Error: Could not write PNG data to url %@: %@", url.path, error);
            }
        }
    }];
}

+ (void)setUserEditedForMetadata:(Metadata *)metadata {
    metadata.userEdited = @YES;
    metadata.source = @(kUser);
    metadata.lastModified = [NSDate date];
}

- (IBAction)addDescription:(id)sender {

    NSAlert *alert = [[NSAlert alloc] init];
    Game *game = [self gameFromObjectID];
    Metadata *metadata = game.metadata;
    NSTextField *entryField = [[NSTextField alloc] initWithFrame:NSMakeRect(0,0, 300, 150)];
    entryField.editable = YES;
    if (metadata.coverArtDescription.length)
        entryField.stringValue = metadata.coverArtDescription;
    else if (metadata.cover.imageDescription.length)
        entryField.stringValue = metadata.cover.imageDescription;

    alert.accessoryView = entryField;
    alert.window.initialFirstResponder = entryField;

    [alert setMessageText:NSLocalizedString(@"Image description", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

    NSModalResponse choice = [alert runModal];

    if (choice == NSAlertFirstButtonReturn && ![entryField.stringValue isEqualToString:metadata.coverArtDescription]) {
        metadata.coverArtDescription = entryField.stringValue;
        metadata.cover.imageDescription = entryField.stringValue;
        [ImageView setUserEditedForMetadata:metadata];
    }

}

#pragma mark Dragging target stuff

- (BOOL)shouldAllowDrag:(id<NSDraggingInfo>)draggingInfo {
    if (draggingInfo.draggingSequenceNumber == _numberForSelfSourcedDrag)
        return NO;
    NSDictionary *filteringOptions = @{ NSPasteboardURLReadingContentsConformToTypesKey:NSImage.imageTypes};

    BOOL canAccept = NO;

    NSPasteboard *pasteBoard = draggingInfo.draggingPasteboard;

    if ([pasteBoard canReadObjectForClasses:@[[NSURL class]] options:filteringOptions]) {
        canAccept = YES;
    } else {
        NSMutableSet *types = [NSMutableSet setWithArray:pasteBoard.types];
        [types intersectSet:ImageView.acceptableDragTypes];
        if (types.count)
            canAccept = YES;
    }

    id dragSourceObject = draggingInfo.draggingSource;
    if ([dragSourceObject isKindOfClass:[ImageDragSource class]])
        dragSourceObject = ((ImageDragSource *)dragSourceObject).sourceView;
    if ([dragSourceObject isKindOfClass:[ImageView class]]) {
        ImageView *source = (ImageView *)dragSourceObject;
        Game *sourcegame = [source gameFromObjectID];
        Game *targetgame = [self gameFromObjectID];
        if ([sourcegame.hashTag isEqualToString:targetgame.hashTag])
            canAccept = NO;
    }

    return canAccept;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    BOOL allow = [self shouldAllowDrag:sender];
    _isReceivingDrag = allow;
    [self updateLayer];
    return allow ? NSDragOperationCopy : NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    _isReceivingDrag = NO;
    [self updateLayer];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    BOOL allow = [self shouldAllowDrag:sender];
    return allow;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)draggingInfo {

    _isReceivingDrag = NO;

    NSArray<Class> *supportedClasses = @[
        [NSURL class], [NSImage class]
    ];

    if (@available(macOS 10.14, *)) {
        supportedClasses = [supportedClasses arrayByAddingObject:[NSFilePromiseReceiver class]];
    }

    // Images dragged from inside the app also carry a temporary file URL
    // (for Terminal). Take the image itself, so the temporary path is not
    // stored as the cover's source and the cover is replaced without asking.
    if ([draggingInfo.draggingSource isKindOfClass:[ImageDragSource class]])
        supportedClasses = @[[NSImage class]];

    NSDictionary<NSPasteboardReadingOptionKey, id> *searchOptions = @{ NSPasteboardURLReadingFileURLsOnlyKey: @YES,
                                                                       NSPasteboardURLReadingContentsConformToTypesKey:imageTypes };

    // Not concurrent: the block reaches Core Data through replaceCoverImage:
    NSDraggingItemEnumerationOptions options = NSDraggingItemEnumerationClearNonenumeratedImages;
    [draggingInfo enumerateDraggingItemsWithOptions:options forView:nil classes:supportedClasses searchOptions:searchOptions usingBlock:^(NSDraggingItem * _Nonnull draggingItem, NSInteger idx, BOOL * _Nonnull stop) {
        if ([draggingItem.item isKindOfClass:[NSFilePromiseReceiver class]] ) {
            NSFilePromiseReceiver *filePromiseReceiver = (NSFilePromiseReceiver *)draggingItem.item;
            [filePromiseReceiver receivePromisedFilesAtDestination:self.destinationURL options:@{} operationQueue:self.workQueue reader:^(NSURL * _Nonnull fileURL, NSError * _Nullable errorOrNil) {
                if (errorOrNil) {
                    NSLog(@"Error: %@", errorOrNil);
                } else {
                    // The reader runs on workQueue, but imageFromURL: reaches
                    // the main context through processImageData:
                    // The received file is only needed until it has been read.
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self imageFromURL:fileURL dontAsk:NO];
                        [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
                    });
                }
            }];
        } else if ([draggingItem.item isKindOfClass:[NSURL class]] ) {
            NSURL *fileURL = (NSURL *)draggingItem.item;
            [self imageFromURL:fileURL dontAsk:NO];
        } else if ([draggingItem.item isKindOfClass:[NSImage class]] ) {
            NSImage *image = (NSImage *)draggingItem.item;
            [self replaceCoverImage:image sourceUrl:@"pasteboard"];
        }
    }];

    return YES;
}

- (BOOL)imageFromURL:(NSURL *)url dontAsk:(BOOL)dontAsk {
    NSData *data = nil;
    if ([Blorb isBlorbURL:url]) {
        // Only accept blorbs with image data but no executable chunk
        // (because it would be confusing to treat game files as image files)
        Blorb *blorb = [Blorb blorbAtURL:url];
        if ([blorb findResourceOfUsage:ExecutableResource] == nil) {
            data = blorb.coverImageData;
            if (data) {
                [self processImageData:data sourceUrl:url.path dontAsk:dontAsk];
                return YES;
            }
        } else {
            return NO;
        }
    }
    data = [NSData dataWithContentsOfURL:url];
    NSImage *image = [[NSImage alloc] initWithData:data];
    if (image) {
        [self processImageData:data sourceUrl:url.path dontAsk:dontAsk];
        return YES;
    } else {
        data = [NSData imageDataFromRetroURL:url];
        if (data) {
            [self processImageData:data sourceUrl:url.path dontAsk:dontAsk];
            return YES;
        }
    }
    return NO;
}


-(void)replaceCoverImage:(NSImage *)image sourceUrl:(NSString *)URLPath {
    NSData *data = image.TIFFRepresentation;
    [self processImageData:data sourceUrl:URLPath dontAsk:NO];
}

-(void)processImageData:(NSData *)imageData sourceUrl:(NSString *)URLPath dontAsk:(BOOL)dontAsk {
    if (!imageData || !URLPath.length)
        return;
    Game *game = [self gameFromObjectID];
    Metadata *metadata = game.metadata;

    Image *oldImageObj = [CoverImageStore fetchImageForURL:URLPath inContext:game.managedObjectContext];
    if (oldImageObj && [(NSData *)oldImageObj.data isEqualToData:imageData]) {
        Image *oldCover = metadata.cover;
        metadata.cover = oldImageObj;
        //If the Image object becomes an orphan, delete it from the Core Data store
        [Image deleteIfOrphan:oldCover];
        metadata.coverArtURL = URLPath;
        return;
    }

    NSData *gameData = (NSData *)game.metadata.cover.data;
    NSString *gamePath = game.path;
    NSManagedObjectContext *context = game.managedObjectContext;

    NSData __block *data = imageData;
    [self.workQueue addOperationWithBlock:^{
        if (data.length > MAX_SPATTERLIGHT_IMAGE_FILE_SIZE)
            data = [data reduceImageDimensionsTo:NSMakeSize(2048, 2048)];

        // Comparing the images for compareByFileNames: is slow, so it is done here rather
        // than on the main thread.
        BOOL replaceWithoutAsking = dontAsk || [URLPath isEqualToString:@"pasteboard"] ||
        [self compareByFileNames:URLPath data:imageData gamePath:gamePath gameData:gameData];

        if (replaceWithoutAsking) {
            [ImageView commitCoverData:data sourceUrl:URLPath toMetadata:metadata context:context];
            return;
        }

        double delayInSeconds = 0.1;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void) {
            ImageCompareViewController *compare = [ImageCompareViewController new];
            // We always replace when pasting
            if ([compare userWantsImage:data ratherThanImage:(NSData *)metadata.cover.data source:kImageComparisonLocalFile force:NO])
                [ImageView commitCoverData:data sourceUrl:URLPath toMetadata:metadata context:context];
        });
    }];
}

// Make data the cover of metadata, as a user edit
+ (void)commitCoverData:(NSData *)data sourceUrl:(NSString *)URLPath toMetadata:(Metadata *)metadata context:(NSManagedObjectContext *)context {
    [context performBlockAndWait:^{
        metadata.coverArtURL = URLPath;
        metadata.coverArtDescription = nil;
        [CoverImageStore insertImageData:data inMetadataID:metadata.objectID context:context];
        [ImageView setUserEditedForMetadata:metadata];
    }];
}

- (BOOL)compareByFileNames:(NSString *)path data:(NSData *)data gamePath:(NSString *)gamePath gameData:(NSData *)gameData {
    if (!gameData) {
        return NO;
    }

    NSString *gameBaseName = gamePath.lastPathComponent.stringByDeletingPathExtension;

    NSString *fileBaseName = path.lastPathComponent.stringByDeletingPathExtension;

    // The fileBaseName may have been given a suffix, such as "image 2.png"
    // if there already was a file named "image.png" on the HDD.
    if (fileBaseName.length > gameBaseName.length && gameBaseName.length > 1) {
        fileBaseName = [fileBaseName substringToIndex:gameBaseName.length];
    }

    return [gameBaseName isEqualToString:fileBaseName] && [ImageSimilarity imageData:gameData looksLike:data];
}

@synthesize workQueue = _workQueue;

- (NSOperationQueue *)workQueue {
    if (_workQueue == nil) {
        _workQueue = [NSOperationQueue new];
        _workQueue.qualityOfService = NSQualityOfServiceUserInitiated;
    }
    return _workQueue;
}

@synthesize destinationURL = _destinationURL;

- (NSURL *)destinationURL {
    if (_destinationURL == nil) {
        _destinationURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"Drops"] isDirectory:YES];
        NSError *error = nil;
        if (![[NSFileManager defaultManager] createDirectoryAtURL:_destinationURL withIntermediateDirectories:YES attributes:nil error:&error]) {
            NSLog(@"Could not create temporary directory at %@: error: %@", _destinationURL.path, error);
            _destinationURL = nil;
        }
    }
    return _destinationURL;
}

#pragma mark Dragging source stuff

- (void)myMouseDown:(NSEvent *)event {
    _isSelected = YES;
    [self.window makeFirstResponder:self];
    [super mouseDown:event];
    [self updateLayer];
}

- (void)mouseDown:(NSEvent *)theEvent {

    [self trackClickOrDragFromEvent:theEvent click:^(NSEvent *upEvent) {
        [self myMouseDown:upEvent];
    } drag:^(NSEvent *dragEvent) {
        if (self->_isPlaceholder || !self->_image)
            return;
        if (!self->dragSource) {
            self->dragSource = [ImageDragSource new];
            self->dragSource.operationQueue = self.workQueue;
        }
        NSDraggingSession *session =
            [self->dragSource dragImage:self->_image
                                   from:self
                                  event:dragEvent
                               filename:[self gameFromObjectID].fileBaseName
                                  frame:self.bounds];
        self->_numberForSelfSourcedDrag = session.draggingSequenceNumber;
    }];
}

- (nullable NSData *)pngData {
    return [ImageDragSource pngDataForImage:self.image];
}

#pragma mark Accessibility

- (BOOL)isAccessibilityElement {
    return YES;
}

- (NSString *)accessibilityRole {
    return NSAccessibilityImageRole;
}

@end

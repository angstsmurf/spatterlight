//
//  ImageView.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-06.
//

#import <QuartzCore/QuartzCore.h>

#import "ImageView.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#import "NSView+snapshot.h"
#import "NSImage+Categories.h"
#import "NSData+Categories.h"
#import "ImageCompareViewController.h"
#import "IFDBDownloader.h"
#import "MyFilePromiseProvider.h"

@interface ImageView ()
{
    NSSet<NSPasteboardType> *nonURLTypes;
}
@end

@implementation ImageView

- (void)updateLayer {
    NSArray *layers = self.layer.sublayers.copy;
    for (CALayer *sub in layers)
        [sub removeFromSuperlayer];
    self.layer.mask = nil;
    if (_isSelected || _isReceivingDrag) {
        // Create a selection border
        CAShapeLayer *shapelayer = [CAShapeLayer layer];

        shapelayer.fillColor = NSColor.clearColor.CGColor;
        shapelayer.frame = self.bounds;

        shapelayer.lineJoin = kCALineJoinRound;
        shapelayer.strokeColor = NSColor.selectedControlColor.CGColor;
        CGFloat lineWidth = 4;
        shapelayer.lineWidth = lineWidth;
        CGRect borderRect = NSMakeRect(lineWidth / 2, lineWidth / 2, self.bounds.size.width - lineWidth, self.bounds.size.height - lineWidth);
        shapelayer.path = CGPathCreateWithRoundedRect(borderRect, 2.5, 2.5, NULL);
        [self.layer addSublayer:shapelayer];
        shapelayer.drawsAsynchronously = YES;

        // Use a mask layer to hide the sharp corners of the image
        // that stick out from the rounded corners of the selection border
        CAShapeLayer *masklayer = [CAShapeLayer layer];
        masklayer.fillColor = NSColor.blackColor.CGColor;
        masklayer.frame = self.bounds;
        masklayer.lineJoin = kCALineJoinRound;
        masklayer.lineWidth = 0;
        borderRect = NSMakeRect(0, 0, masklayer.frame.size.width,  masklayer.frame.size.height);
        masklayer.path = CGPathCreateWithRoundedRect(borderRect, 5, 5, NULL);
        [self.layer addSublayer:masklayer];
        self.layer.mask = masklayer;
    }
}

- (BOOL) wantsUpdateLayer {
    return YES;
}

- (instancetype)initWithGame:(Game *)game image:(nullable NSImage *)anImage {
    if (!anImage && game.metadata.cover.data)
        anImage = [[NSImage alloc] initWithData:(NSData *)game.metadata.cover.data];
    if (anImage)
        self = [super initWithFrame:NSMakeRect(0, 0, anImage.size.width, anImage.size.height)];
    else
        self = [super initWithFrame:NSZeroRect];
    if (self) {
        _image = anImage;
        _game = game;

        self.enabled = YES;
        nonURLTypes = [NSSet setWithObjects:NSPasteboardTypeTIFF, NSPasteboardTypePNG, nil];
        _acceptableTypes = [NSSet setWithObject:NSURLPboardType];
        _acceptableTypes = [_acceptableTypes setByAddingObjectsFromSet:nonURLTypes];

        [self registerForDraggedTypes:_acceptableTypes.allObjects];

        _numberForSelfSourcedDrag = NSNotFound;

        self.accessibilityLabel = game.metadata.coverArtDescription;
        if (!self.accessibilityLabel.length)
            self.accessibilityLabel = [NSString stringWithFormat:@"cover image for %@", game.metadata.title];
        if (_image)
            [self processImage:_image];
        [self updateLayer];

    }
    return self;
}

- (void)processImage:(NSImage *)image {
    CALayer *layer = [CALayer layer];

    NSImageRep *rep = [[_image representations] objectAtIndex:0];
    NSSize sizeInPixels = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);

    layer.magnificationFilter = sizeInPixels.width < 350 ? kCAFilterNearest : kCAFilterTrilinear;

//    NSLog(@"sizeInPixels: %@ magnificationFilter: %@", NSStringFromSize(sizeInPixels), sizeInPixels.width < 350 ? @"kCAFilterNearest" : @"kCAFilterTrilinear");

    layer.drawsAsynchronously = YES;
    layer.contentsGravity = kCAGravityResize;

    layer.contents = image;

    self.layer = layer;
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


- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {

    if (menuItem.action == @selector(cut:) || menuItem.action == @selector(copy:) || menuItem.action == @selector(delete:) || menuItem.action == @selector(paste:)) {

        return YES;
    }

    return NO;
}

- (void)cut:(id)sender {
    NSLog(@"cut:");
    [self copy:nil];
    //Delete the cover relation of the Metadata object
    Image *image = _game.metadata.cover;
    _game.metadata.cover = nil;
    //If the Image object becomes an orphan, delete it from the Core Data store
    if (image.metadata.count == 0)
        [_game.managedObjectContext deleteObject:image];
}

- (void)copy:(id)sender {
    NSLog(@"copy:");
    NSPasteboard *pb = [NSPasteboard  generalPasteboard];
    [pb clearContents];
    [pb writeObjects:@[_image]];
}

- (void)delete:(id)sender {
    NSLog(@"delete:");
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"Are you sure?", nil);
        alert.informativeText = NSLocalizedString(@"Do you want to delete this cover image?", nil);
        alert.icon = _image;
        [alert addButtonWithTitle:NSLocalizedString(@"Delete", nil)];
        [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];

        NSInteger choice = [alert runModal];

        if (choice == NSAlertFirstButtonReturn) {
            //Delete the cover relation of the Metadata object
            Image *image = _game.metadata.cover;
            _game.metadata.cover = nil;
            //If the Image object becomes an orphan, delete it from the Core Data store
            if (image.metadata.count == 0)
                [_game.managedObjectContext deleteObject:image];
        }
}

- (void)paste:(id)sender {
    NSLog(@"paste:");
    NSPasteboard *pb = [NSPasteboard  generalPasteboard];

    NSArray *objects = [pb readObjectsForClasses:@[[NSURL class], [NSImage class]] options:nil];

    for (id item in objects) {
        if ([item isKindOfClass:[NSImage class]]) {
            [self replaceCoverImage:(NSImage *)item];
            break;
        } else if ([item isKindOfClass:[NSURL class]]) {
            NSImage *image = [[NSImage alloc] initWithContentsOfURL:(NSURL *)item];
            if (image) {
                [self replaceCoverImage:image];
                break;
            }
        }
    }
}

- (BOOL)shouldAllowDrag:(id<NSDraggingInfo>)draggingInfo {
    if (draggingInfo.draggingSequenceNumber == _numberForSelfSourcedDrag)
        return NO;
    NSDictionary *filteringOptions = @{ NSPasteboardURLReadingContentsConformToTypesKey:[NSImage imageTypes]};

    BOOL canAccept = NO;

    NSPasteboard *pasteBoard = draggingInfo.draggingPasteboard;

    if ([pasteBoard canReadObjectForClasses:@[[NSURL class]] options:filteringOptions]) {
        canAccept = YES;
    } else {
        NSMutableSet *types = [NSMutableSet setWithArray:pasteBoard.types];
        [types intersectSet:_acceptableTypes];
        if (types.count)
            canAccept = YES;
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
    [self setNeedsDisplay:YES];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
    BOOL allow = [self shouldAllowDrag:sender];
    [self setNeedsDisplay:YES];
    return allow;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)draggingInfo {
    NSArray *types = [NSImage imageTypes];
    types = [types arrayByAddingObjectsFromArray:@[@"public.neochrome", @"public.mcga", @"public.dat"]];
    NSDictionary *filteringOptions = @{ NSPasteboardURLReadingContentsConformToTypesKey:types};

    _isReceivingDrag = NO;
    NSPasteboard *pasteBoard = draggingInfo.draggingPasteboard;

    NSArray<NSURL *> *urls = [pasteBoard readObjectsForClasses:@[[NSURL class]] options:filteringOptions];

    NSImage *image;
    if (urls.count == 1) {
        NSURL *url = urls.firstObject;
        image = [[NSImage alloc] initWithContentsOfURL:url];
        if (image) {
            [self replaceCoverImage:image];
            return YES;
        } else {
            NSData *data = [NSData imageDataFromRetroURL:url];
            if (data) {
                [self processImageData:data];
                return YES;
            }
        }
    } else {
        image = [[NSImage alloc] initWithPasteboard:pasteBoard];
        if (image) {
            [self replaceCoverImage:image];
            return YES;
        }
    }

    return NO;
}

-(void)replaceCoverImage:(NSImage *)image {
    NSData *data = image.TIFFRepresentation;
    [self processImageData:data];
}

-(void)processImageData:(NSData *)image {
    double delayInSeconds = 0.1;
    Metadata *metadata = _game.metadata;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        ImageCompareViewController *compare = [ImageCompareViewController new];
        if ([compare userWantsImage:image ratherThanImage:(NSData *)metadata.cover.data]) {
            IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithContext:metadata.managedObjectContext];
            [downloader insertImage:image inMetadata:metadata];
        }
    });
}


#pragma mark Source stuff

- (NSDragOperation)draggingSession:(NSDraggingSession *)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
    /*------------------------------------------------------
     NSDraggingSource protocol method.  Returns the types of operations allowed in a certain context.
     --------------------------------------------------------*/
    switch (context) {
        case NSDraggingContextOutsideApplication:
            return NSDragOperationCopy;

            //by using this fall through pattern, we will remain compatible if the contexts get more precise in the future.
        case NSDraggingContextWithinApplication:
        default:
            return NSDragOperationCopy;
            break;
    }
}

- (void)mouseDown:(NSEvent*)event {
    _isSelected = YES;
    [self.window makeFirstResponder:self];
    [super mouseDown:event];
   [self updateLayer];
}


- (void)mouseDragged:(NSEvent*)event
{
    MyFilePromiseProvider *provider = [[MyFilePromiseProvider alloc] initWithFileType: NSPasteboardTypePNG delegate:self];

    NSDraggingItem *dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:provider];

    for (CALayer *sub in self.layer.sublayers)
        [sub removeFromSuperlayer];
    NSImage *snapshot = [self snapshot];
    [self updateLayer];

    [dragItem setDraggingFrame:self.bounds contents:snapshot];

    NSDraggingSession *session = [self beginDraggingSessionWithItems:@[dragItem] event:event source:self];
    _numberForSelfSourcedDrag = session.draggingSequenceNumber;
}


- (NSString *)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider
                  fileNameForType:(NSString *)fileType {
    NSString *fileName = [_game.path.lastPathComponent.stringByDeletingPathExtension stringByAppendingPathExtension:@"png"];
    if (!fileName.length)
        fileName = @"image.png";

    return fileName;
}

- (void)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider
          writePromiseToURL:(NSURL *)url
          completionHandler:(void (^)(NSError *errorOrNil))completionHandler {

    NSData *bitmapData = [self pngData];


    NSError *error = nil;

    if (![bitmapData writeToFile:url.path atomically:YES]) {
        NSLog(@"Error: %@", error);
//        completionHandler(error);

        // TODO handle the error
    }

    completionHandler(nil);
    NSLog(@"Your image has been saved to %@", url.path);
}


- (NSData *)pngData {
    NSBitmapImageRep *bitmaprep = [self.image bitmapImageRepresentation];

    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[bitmaprep] usingType:NSBitmapImageFileTypePNG properties:props];
}

- (void)pasteboard:(nullable NSPasteboard *)pasteboard item:(nonnull NSPasteboardItem *)item provideDataForType:(nonnull NSPasteboardType)type {

    NSLog(@"pasteboard %@ item %@ provideDataForType: %@", pasteboard, item, type);

    if ( [type isEqual: NSPasteboardTypeTIFF]) {

        //set data for TIFF type on the pasteboard as requested
        [pasteboard setData:[[self image] TIFFRepresentation] forType:NSPasteboardTypeTIFF];

    } else if ( [type isEqual: NSPasteboardTypePDF] ) {

        //set data for PDF type on the pasteboard as requested
        [pasteboard setData:[self dataWithPDFInsideRect:[self bounds]] forType:NSPasteboardTypePDF];
    } else if ( [type isEqual:  NSPasteboardTypePNG] ) {

        //set data for PDF type on the pasteboard as requested
        [pasteboard setData:[self pngData] forType:NSPasteboardTypePNG];
    }
}


@end

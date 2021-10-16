//
//  PreviewViewController.m
//  SpatterlightQuickLook
//
//  Created by Administrator on 2021-01-29.
//

#import <Cocoa/Cocoa.h>
#import <Quartz/Quartz.h>
#import <CoreData/CoreData.h>
#import <CoreSpotlight/CoreSpotlight.h>
#import <BlorbFramework/BlorbFramework.h>

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "Ifid.h"

#import "NSDate+relative.h"

#import "iFictionPreviewController.h"
#import "PreviewViewController.h"
#import "InfoView.h"

/* the treaty of babel headers */
#include "babel_handler.h"

@interface PreviewViewController () <QLPreviewingController>
{
    BOOL iFiction;
}

@end

@implementation PreviewViewController

- (NSString *)nibName {
    return @"PreviewViewController";
}

- (void)loadView {
    [super loadView];
    if (@available(macOS 11, *)) {
        NSSize size;
        if (iFiction)
            size = NSMakeSize(820, 846);
        else
            size = NSMakeSize(575, 285);
        
        self.preferredContentSize = size;
    }
}

#pragma mark - Core Data stack

@synthesize persistentContainer = _persistentContainer;

- (NSPersistentContainer *)persistentContainer {
    @synchronized (self) {
        if (_persistentContainer == nil) {
            _persistentContainer = [[NSPersistentContainer alloc] initWithName:@"Spatterlight"];
            
            NSString *groupIdentifier =
            [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];
            
            NSURL *url = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];
            
            url = [url URLByAppendingPathComponent:@"Spatterlight.storedata"];
            
            NSPersistentStoreDescription *description = [[NSPersistentStoreDescription alloc] initWithURL:url];
            
            description.readOnly = YES;
            description.shouldMigrateStoreAutomatically = NO;
            
            _persistentContainer.persistentStoreDescriptions = @[ description ];
            
            [_persistentContainer loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *aDescription, NSError *error) {
                if (error != nil) {
                    NSLog(@"Failed to load Core Data stack: %@", error);
                }
            }];
        }
    }
    return _persistentContainer;
}

/*
 * Implement this method and set QLSupportsSearchableItems to YES in the Info.plist of the extension if you support CoreSpotlight.
 */
- (void)preparePreviewOfSearchableItemWithIdentifier:(NSString *)identifier queryString:(NSString *)queryString completionHandler:(void (^)(NSError * _Nullable))handler {
    NSLog(@"preparePreviewOfSearchableItemWithIdentifier: %@ queryString: %@", identifier, queryString );
    
    _ifid = nil;
    _addedFileInfo = NO;
    _showingIcon = NO;
    _showingView = NO;
    
    // Add the supported content types to the QLSupportedContentTypes array in the Info.plist of the extension.
    
    // Perform any setup necessary in order to prepare the view.
    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.
    
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSError *contexterror = [NSError errorWithDomain:NSCocoaErrorDomain code:6 userInfo:@{
            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Could not create new background context\n"]}];
        NSLog(@"Could not create new context!");
        handler(contexterror);
        return;
    }
    
    __block NSURL *url = nil;
    
    __block Game *game = nil;
    __block Metadata *metadata = nil;
    __block Image *image = nil;
    
    __block bool giveUp = NO;
    
    [context performBlockAndWait:^{
        
        NSError *error;
        
        NSURL *uri = [NSURL URLWithString:identifier];
        if (!uri) {
            error = [NSError errorWithDomain:NSCocoaErrorDomain code:7 userInfo:@{
                NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Could not create URI from identifier string %@\n", identifier]}        ];
            handler(error);
            giveUp = YES;
            return;
        }
        
        NSManagedObjectID *objectID = [context.persistentStoreCoordinator managedObjectIDForURIRepresentation:uri];
        
        
        if (!objectID) {
            error = [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:@{
                NSLocalizedDescriptionKey: [NSString stringWithFormat:@"This game has been deleted from the Spatterlight database.\n"]}];
            handler(error);
            giveUp = YES;
            return;
        }
        
        id object = [context objectWithID:objectID];
        
        if (!object) {
            error = [NSError errorWithDomain:NSCocoaErrorDomain code:2 userInfo:@{
                NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Could not create object from identifier %@\n", identifier]}];
            handler(error);
            giveUp = YES;
            return;
        }
        
        NSLog(@"Object is of kind %@", [object class]);
        
        if ([object isKindOfClass:[Metadata class]]) {
            metadata = (Metadata *)object;
            game = metadata.games.anyObject;
        } else if ([object isKindOfClass:[Game class]]) {
            game = (Game *)object;
            metadata = game.metadata;
        } else if ([object isKindOfClass:[Image class]]) {
            image = (Image *)object;
            metadata = image.metadata.anyObject;
            game = metadata.games.anyObject;
            if (!metadata)
                NSLog(@"Image has no metadata!");
            if (!game)
                NSLog(@"Image has no game!");
            if (!image.data)
                NSLog(@"Image has no data!");
        } else {
            error = [NSError errorWithDomain:NSCocoaErrorDomain code:3 userInfo:@{
                NSLocalizedDescriptionKey: [NSString stringWithFormat:@"No support for class %@\n", [object class]]}];
            handler(error);
            giveUp = YES;
            return;
        }
        
    }];
    
    if (giveUp) {
        CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
        [index deleteSearchableItemsWithIdentifiers:@[identifier]
                                  completionHandler:^(NSError *blockerror){
            if (blockerror) {
                NSLog(@"Deleting searchable item failed: %@", blockerror);
            } else {
                NSLog(@"Successfully deleted searchable item");
            }
        }];
        return;
    }
    
    if (image && !image.data) {
        NSError *error = [NSError errorWithDomain:NSCocoaErrorDomain code:102 userInfo:@{
            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Image has no data!?!\n"]}];
        handler(error);
        return;
    }
    
    if (!metadata) {
        NSError *error = [NSError errorWithDomain:NSCocoaErrorDomain code:101 userInfo:@{
            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"No metadata!?!\n"]}];
        handler(error);
        return;
    }
    
    [_imageView removeFromSuperview];
    
    NSScrollView *scrollView = _textview.enclosingScrollView;
    scrollView.hasVerticalScroller = YES;
    scrollView.frame = NSMakeRect(0, 0, 390, 378);
    
    InfoView *infoView = [[InfoView alloc] initWithFrame:scrollView.bounds];
    
    if (game)
        url = [game urlForBookmark];
    
    if (!metadata.cover.data && metadata.blurb.length == 0 && metadata.author.length == 0 && metadata.headline.length == 0 && url) {
        Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
        if (blorb)
            infoView.imageData = [blorb coverImageData];
        if (!infoView.imageData) {
            _showingIcon = YES;
            NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
            infoView.imageData = icon.TIFFRepresentation;
        }
    }
    
    NSInteger rating = NSNotFound;
    if (metadata.starRating.length) {
        rating = metadata.starRating.integerValue;
    } else if  (metadata.myRating.length) {
        rating = metadata.myRating.integerValue;
    }
    infoView.starString = [PreviewViewController starString:rating];
    
    scrollView.documentView = infoView;
    if (image) {
        infoView.imageData = (NSData *)image.data;
        [infoView updateWithImage:image];
    } else {
        [infoView updateWithMetadata:metadata];
    }
    
    handler(nil);
}

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {
    
    _ifid = nil;
    _addedFileInfo = NO;
    _showingIcon = NO;
    _showingView = NO;
    iFiction = NO;
    // Add the supported content types to the QLSupportedContentTypes array in the Info.plist of the extension.
    
    // Perform any setup necessary in order to prepare the view.
    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.
    
    NSString *extension = url.pathExtension.lowercaseString;
    
    if ([extension isEqualToString:@"ifiction"]) {
        iFiction = YES;
        [_imageView removeFromSuperview];
        iFictionPreviewController *iFictionController = [iFictionPreviewController new];
        iFictionController.textview  = _textview;
        NSScrollView *scrollView = _textview.enclosingScrollView;
        scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollView.frame = self.view.bounds;
        _textview.frame = self.view.bounds;
        
        if (@available(macOS 11, *)) {
            self.preferredContentSize = NSMakeSize(820, 846);
        }
        
        [iFictionController preparePreviewOfFileAtURL:url completionHandler:handler];
        return;
    }
    
    if ([extension isEqualToString:@"glkdata"] || [extension isEqualToString:@"glksave"] || [extension isEqualToString:@"qut"] || [extension isEqualToString:@"d$$"]) {
        [self noPreviewForURL:url handler:handler];
        return;
    }
    
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"Could not create new context!");
        [self noPreviewForURL:url handler:handler];
        return;
    }
    
    __block NSMutableDictionary *metadata = nil;
    
    __unsafe_unretained PreviewViewController *weakSelf = self;
    [context performBlockAndWait:^{
        PreviewViewController *strongSelf = weakSelf;
        NSError *error = nil;
        NSArray *fetchedObjects;
        
        NSFetchRequest *fetchRequest = [NSFetchRequest new];
        
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path like[c] %@", url.path];
        
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects == nil) {
            NSLog(@"QuickLook: %@",error);
            [strongSelf noPreviewForURL:url handler:handler];
            return;
        }
        if (fetchedObjects.count == 0) {
            strongSelf.ifid = [strongSelf ifidFromFile:url.path];
            if (strongSelf.ifid) {
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@", strongSelf.ifid];
                fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
            }
            if (fetchedObjects.count == 0) {
                metadata = [strongSelf metadataFromURL:url];
                if (metadata == nil || metadata.count == 0) {
                    [strongSelf noPreviewForURL:url handler:handler];
                    return;
                }
            }
        }
        
        
        if (metadata == nil || metadata.count == 0) {
            
            Game *game = fetchedObjects[0];
            Metadata *meta = game.metadata;
            
            NSDictionary *attributes = [NSEntityDescription
                                        entityForName:@"Metadata"
                                        inManagedObjectContext:context].attributesByName;
            
            metadata = [[NSMutableDictionary alloc] initWithCapacity:attributes.count];
            
            for (NSString *attr in attributes) {
                [metadata setValue:[meta valueForKey:attr] forKey:attr];
            }
            metadata[@"ifid"] = game.ifid;
            metadata[@"cover"] = game.metadata.cover.data;
            metadata[@"lastPlayed"] = game.lastPlayed;
        }
        
        if (!metadata.count) {
            [strongSelf noPreviewForURL:url handler:handler];
            return;
        }
    }];
    
    if (metadata[@"cover"]) {
        _imageView.image = [[NSImage alloc] initWithData:(NSData *)metadata[@"cover"]];
        _imageView.accessibilityLabel = metadata[@"coverArtDescription"];
    } else {
        Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
        _imageView.image = [[NSImage alloc] initWithData:[blorb coverImageData]];
        if (!_imageView.image) {
            _showingIcon = YES;
            _imageView.image = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
        }
    }
    
    [self updateWithMetadata:metadata url:url];
    if (metadata.count <= 2 && url) {
        [self addFileInfo:url];
    }
    
    [self finalAdjustments:handler];
    
}

- (void)noPreviewForURL:(NSURL *)url handler:(void (^)(NSError *))handler {
    NSFont *systemFont = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
    NSMutableDictionary *attrDict = [NSMutableDictionary new];
    attrDict[NSFontAttributeName] = systemFont;
    attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
    [self addInfoLine:url.path.lastPathComponent attributes:attrDict linebreak:NO];
    
    attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    
    if ([url.path.pathExtension.lowercaseString isEqualToString:@"d$$"]) {
        [self addInfoLine:@"Possibly an AGT game. Try opening in Spatterlight to convert to AGX format." attributes:attrDict linebreak:YES];
    } else if ([url.path.pathExtension.lowercaseString isEqualToString:@"glksave"] || [url.path.pathExtension.lowercaseString isEqualToString:@"qut"]) {
        NSString *saveTitle = [self titleFromSave:url.path];
        if (saveTitle.length)
            [self addInfoLine:[NSString stringWithFormat:@"Save file associated with game %@.", saveTitle] attributes:attrDict linebreak:YES];
        else {
            [self addInfoLine:@"Interactive fiction save file." attributes:attrDict linebreak:YES];
        }
    } else if ([url.path.pathExtension.lowercaseString isEqualToString:@"glkdata"]) {
        NSString *dataTitle = [self titleFromDataFile:url.path];
        if (dataTitle.length)
            [self addInfoLine:[NSString stringWithFormat:@"Data file associated with game %@.", dataTitle] attributes:attrDict linebreak:YES];
        else
            [self addInfoLine:@"Interactive fiction data file." attributes:attrDict linebreak:YES];
    }
    
    if (_ifid.length) {
        [self addInfoLine:[@"IFID: " stringByAppendingString:_ifid] attributes:attrDict linebreak:YES];
    }
    
    [self addFileInfo:url];
    
    if (!_imageView.image) {
        _showingIcon = YES;
        _imageView.image = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
    }
    
    [self finalAdjustments:handler];
}

-(void)finalAdjustments:(void (^)(NSError * _Nullable))handler  {
    NSScrollView *scrollView = _textview.enclosingScrollView;
    NSView *superView = _imageView.superview;
    __block NSSize viewSize = superView.frame.size;
    __unsafe_unretained PreviewViewController *weakSelf = self;
    
    double delayInSeconds = 0.2;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        
        PreviewViewController *strongSelf = weakSelf;
        if (strongSelf) {
            viewSize = superView.frame.size;
            strongSelf.vertical = (viewSize.width - viewSize.height <= 20);
            [strongSelf sizeImage];
            [strongSelf sizeText];
            CGFloat scrollheight = scrollView.frame.size.height;
            CGFloat textheight = strongSelf.textview.frame.size.height;
            CGFloat viewheight = superView.frame.size.height;
            [scrollView.contentView scrollToPoint:NSZeroPoint];
            if (!strongSelf.vertical) {
                if (textheight < viewheight - 40 && scrollheight < textheight) {
                    NSRect frame = strongSelf.textview.enclosingScrollView.frame;
                    //Text is mysteriously cropped at the bottom
                    CGFloat diff = textheight - scrollheight;
                    frame.size.height = textheight;
                    frame.origin.y -= diff;
                    scrollView.frame = frame;
                    scrollView.contentView.frame = scrollView.bounds;
                }
            }
        }
    });
    
    delayInSeconds = 0.3;
    popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        self.showingView = YES;
        handler(nil);
    });
    
    delayInSeconds = 0.4;
    popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [self sizeImage];
    });
}


-(void)viewWillLayout {
    if (!_showingView || iFiction)
        return;
    NSSize viewSize = self.view.frame.size;
    _vertical = (viewSize.width - viewSize.height <= 20);
    [self sizeImage];
    [self sizeText];
}

- (void)printFinalLayout{
    NSLog(@"Final layout: view frame: %@ subview frame:%@ image view frame %@ scroll view frame:%@ text view frame:%@", NSStringFromRect(self.view.frame), NSStringFromRect(_imageView.superview.frame), NSStringFromRect(_imageView.frame), NSStringFromRect(_textview.enclosingScrollView.frame), NSStringFromRect(_textview.frame));
    NSLog(@"Layout is %@", _vertical ? @"vertical" : @"horizontal");
    NSLog(@"Showing icon: %@", _showingIcon ? @"YES" : @"NO");
    NSLog(@"_textview.string.length:%ld", _textview.string.length);
    
}

- (void)sizeImageToFitWidth:(CGFloat)maxWidth height:(CGFloat)maxHeight {
    
    NSSize imgsize = _originalImageSize;
    if (imgsize.width == 0 || imgsize.height == 0) {
        imgsize = _imageView.image.size;
        _originalImageSize = imgsize;
    }
    
    if (imgsize.width == 0 || imgsize.height == 0)
        return;
    
    CGFloat ratio = imgsize.width / imgsize.height;
    
    imgsize.height = maxHeight;
    imgsize.width = imgsize.height * ratio;
    
    if (imgsize.width > maxWidth) {
        imgsize.width = maxWidth;
        imgsize.height =  imgsize.width / ratio;
    }
    
    _imageView.image.size = imgsize;
}

- (void)sizeImage {
    if (!_imageView.image)
        return;
    if (!_showingView) {
        _hasSized++;
        if (_hasSized > 10)
            return;
    }
    BOOL wasShowingView = _showingView;
    _showingView = NO;
    if (!_vertical) {
        [self sizeImageHorizontally];
    } else [self sizeImageVertically];
    
    _showingView = wasShowingView;
    
    if (!_showingView)
        [self imageShadow];
}

- (void)imageShadow {
    NSShadow *shadow = [NSShadow new];
    shadow.shadowOffset = NSMakeSize(2, -2);
    shadow.shadowColor = [NSColor controlShadowColor];
    shadow.shadowBlurRadius = 2;
    NSImageView *imageView = _imageView;
    imageView.wantsLayer = YES;
    imageView.superview.wantsLayer = YES;
    imageView.shadow = shadow;
}

- (void)sizeImageHorizontally {
    NSImageView *imageView = _imageView;
    NSSize viewSize = imageView.superview.frame.size;
    
    [self sizeImageToFitWidth:round(viewSize.width / 2 - 40) height:viewSize.height - 40];
    NSRect frame = imageView.frame;
    frame.size = imageView.image.size;
    NSRect scrollFrame = _textview.enclosingScrollView.frame;
    CGFloat textHeight = scrollFrame.size.height;
    if (textHeight != 0 && frame.size.height < textHeight) {
        frame.origin.y = NSMaxY(scrollFrame) - frame.size.height;
    } else {
        frame.origin.y = round((viewSize.height - frame.size.height) / 2);
        if (frame.origin.y < 20)
            frame.origin.y = 20;
    }
    frame.origin.x = 20;
    if (frame.origin.y < 0)
        if (frame.origin.y == 0)
            if (frame.size.width < 1)
                frame.size.width = 40;
    if (frame.size.height < 1)
        frame.size.width = 40;
    
    imageView.frame = frame;
}

- (void)sizeImageVertically {
    NSImageView *imageView = _imageView;
    NSSize viewSize = imageView.superview.frame.size;
    
    //We want the image to be at most two thirds of the view height
    [self sizeImageToFitWidth:viewSize.width - 40 height:round(viewSize.height - 40)];
    
    NSRect frame = imageView.frame;
    frame.size.height = imageView.image.size.height;
    frame.size.width = viewSize.width - 40;
    frame.origin.y = round((viewSize.height - frame.size.height) / 2);
    frame.origin.x = 20;
    if (!NSEqualRects(frame, imageView.frame))
        imageView.frame = frame;
    imageView.imageAlignment =  NSImageAlignCenter;
}

- (void)sizeText {
    if (!_showingView) {
        _hasSized++;
        if (_hasSized > 5)
            return;
    }
    NSRect frame = _textview.enclosingScrollView.frame;
    BOOL wasShowingView = _showingView;
    _showingView = NO;
    if (!_vertical) {
        [self sizeTextHorizontally];
    } else [self sizeTextVertically];
    if (!NSEqualRects(frame, _textview.enclosingScrollView.frame))
        [_textview.layoutManager ensureLayoutForTextContainer:_textview.textContainer];
    _showingView = wasShowingView;
}

- (void)sizeTextHorizontally {
    NSImageView *imageView = _imageView;
    //    NSLog(@"sizeTextHorizontally");
    NSScrollView *scrollView = _textview.enclosingScrollView;
    NSRect frame = scrollView.frame;
    NSSize viewSize = self.view.frame.size;
    
    // Icon images usually has horizontal padding built-in
    // so we use a little less space here
    frame.origin.x = round(NSMaxX(imageView.frame) + (_showingIcon ? 10 : 20));
    frame.size.width = round(viewSize.width - frame.origin.x - 20);
    
    CGFloat textHeight = [self heightForString:_textview.textStorage andWidth:frame.size.width];
    
    if (textHeight < viewSize.height - 50) {
        CGFloat scrollViewHeight = MIN(ceil(textHeight + _textview.textContainerInset.height * 2 + _textview.textContainer.lineFragmentPadding * 2), viewSize.height - 20);
        frame.size.height = scrollViewHeight;
        frame.origin.y = ceil((viewSize.height - textHeight) / 2);
        if (frame.origin.y < 0)
            frame.origin.y = 0;
    } else {
        frame.size.height = round(viewSize.height - 40);
        if (frame.size.height < 0)
            frame.size.height = 5;
        frame.origin.y = 20;
    }
    
    if (!NSEqualRects(frame, scrollView.frame))
        scrollView.frame = frame;
    frame = _textview.frame;
    frame.size.width = scrollView.frame.size.width;
    if (!NSEqualRects(frame, _textview.frame))
        _textview.frame = frame;
    
    if (imageView.image.size.height < textHeight && NSMaxY(imageView.frame) < NSMaxY(scrollView.frame)) {
        frame = imageView.frame;
        frame.origin.y = NSMaxY(scrollView.frame) - frame.size.height;
        if (!NSEqualRects(frame, imageView.frame)) {
            imageView.frame = frame;
            imageView.imageAlignment = NSImageAlignTop;
        }
    }
    _textview.hidden = NO;
}

- (void)sizeTextVertically {
    //    NSLog(@"sizeTextVertically");
    _textview.hidden = YES;
}

- (CGFloat)heightForString:(NSAttributedString *)attString andWidth:(CGFloat)textWidth {
    
    CGFloat padding = _textview.textContainer.lineFragmentPadding;
    
    NSTextStorage *textStorage = [[NSTextStorage alloc] initWithAttributedString:attString];
    NSTextContainer *textContainer = [[NSTextContainer alloc]
                                      initWithContainerSize:NSMakeSize(textWidth + padding * 2, FLT_MAX)];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];
    textContainer.lineFragmentPadding = padding;
    
    [layoutManager glyphRangeForTextContainer:textContainer];
    
    CGRect proposedRect =
    [layoutManager usedRectForTextContainer:textContainer];
    return proposedRect.size.height + 10;
}

- (void)updateWithMetadata:(NSDictionary *)metadict url:(nullable NSURL *)url {
    if (metadict && metadict.count) {
        NSFont *systemFont = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
        NSMutableDictionary *attrDict = [NSMutableDictionary new];
        attrDict[NSFontAttributeName] = systemFont;
        attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
        [self addInfoLine:metadict[@"title"] attributes:attrDict linebreak:NO];
        
        if (!metadict[@"title"]) {
            [self addInfoLine:url.path.lastPathComponent attributes:attrDict linebreak:NO];
            
            if (metadict[@"SNam"]) {
                NSMutableDictionary *metamuta = metadict.mutableCopy;
                metamuta[@"IFhd"] = @"dummyIFID";
                metamuta[@"IFhdTitle"] = metadict[@"SNam"];
                metadict = metamuta;
                NSLog(@"Set IFhdTitle to %@", metadict[@"SNam"]);
            }
            
            if (metadict[@"IFhd"]) {
                attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
                NSString *resBlorbStr = @"Resource associated with game\n";
                if (metadict[@"IFhdTitle"])
                    resBlorbStr = [resBlorbStr stringByAppendingString:metadict[@"IFhdTitle"]];
                else
                    resBlorbStr = [resBlorbStr stringByAppendingString:metadict[@"IFhd"]];
                [self addInfoLine:resBlorbStr attributes:attrDict linebreak:YES];
            }
        }
        
        BOOL noMeta = (!((NSString *)metadict[@"headline"]).length && !((NSString *)metadict[@"author"]).length && !((NSString *)metadict[@"blurb"]).length);
        
        [self addStarRating:metadict];
        attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        [self addInfoLine:metadict[@"headline"] attributes:attrDict linebreak:YES];
        [self addInfoLine:metadict[@"author"] attributes:attrDict linebreak:YES];
        if (!metadict[@"author"])
            [self addInfoLine:metadict[@"AUTH"] attributes:attrDict linebreak:YES];
        [self addInfoLine:metadict[@"blurb"] attributes:attrDict linebreak:YES];
        
        [self addInfoLine:metadict[@"ANNO"] attributes:attrDict linebreak:YES];
        if (metadict[@"(c) "])
            [self addInfoLine:[NSString stringWithFormat:@"© %@", metadict[@"(c) "]] attributes:attrDict linebreak:YES];
        
        NSDate * lastPlayed = metadict[@"lastPlayed"];
        NSString *lastPlayedString = [self formattedStringFromDate:lastPlayed];
        // It looks better with the last played line last if we show file info,
        // so that modification date follows last played date and the two dates are grouped
        if (lastPlayed && !noMeta) {
            [self addInfoLine:[NSString stringWithFormat:@"Last played: %@", lastPlayedString] attributes:attrDict linebreak:YES];
        }
        
        if (!noMeta) {
            attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
        } else {
            attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        }
        if  (metadict[@"ifid"])
            [self addInfoLine:[@"IFID: " stringByAppendingString:metadict[@"ifid"]] attributes:attrDict linebreak:YES];
        
        // See comment above
        if (lastPlayed && noMeta)
            [self addInfoLine:[NSString stringWithFormat:@"Last played: %@", lastPlayedString] attributes:attrDict linebreak:YES];
        if (noMeta && url)
            [self addFileInfo:url];
    }
}

- (void)addInfoLine:(NSString *)string attributes:(NSDictionary *)attrDict linebreak:(BOOL)linebreak {
    if (string == nil || string.length == 0)
        return;
    NSTextStorage *textstorage = _textview.textStorage;
    if (linebreak)
        string = [@"\n\n" stringByAppendingString:string];
    NSAttributedString *attString = [[NSAttributedString alloc] initWithString:string attributes:attrDict];
    [textstorage appendAttributedString:attString];
}

- (void)addStarRating:(NSDictionary *)dict {
    NSInteger rating = NSNotFound;
    if (dict[@"myRating"]) {
        rating = ((NSNumber *)dict[@"myRating"]).integerValue;
    } else if  (dict[@"starRating"]) {
        rating = ((NSNumber *)dict[@"starRating"]).integerValue;
    }
    NSAttributedString *starString = [PreviewViewController starString:rating];
    [_textview.textStorage appendAttributedString:starString];
}

+ (nullable NSAttributedString *)starString:(NSInteger)rating {
    
    NSMutableAttributedString *starString = [NSMutableAttributedString new];
    
    if (rating == NSNotFound)
        return starString;
    
    NSUInteger totalNumberOfStars = 5;
    NSFont *currentFont = [NSFont fontWithName:@"SF Pro" size:12];
    if (!currentFont)
        currentFont = [NSFont systemFontOfSize:20 weight:NSFontWeightRegular];
    
    NSMutableParagraphStyle *para = [NSMutableParagraphStyle new];
    para.alignment = NSCenterTextAlignment;
    
    if (@available(macOS 10.13, *)) {
        NSDictionary *activeStarFormat = @{
            NSFontAttributeName : currentFont,
            NSForegroundColorAttributeName : [NSColor colorNamed:@"customControlColor"],
        };
        NSDictionary *inactiveStarFormat = @{
            NSFontAttributeName : currentFont,
            NSForegroundColorAttributeName : [NSColor colorNamed:@"customControlColor"]
        };
        
        [starString appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:@"\n\n" attributes:activeStarFormat]];
        
        for (int i=0; i < totalNumberOfStars; ++i) {
            //Full star
            if (rating >= i+1) {
                [starString appendAttributedString:[[NSAttributedString alloc]
                                                    initWithString:NSLocalizedString(@"􀋃 ", nil) attributes:activeStarFormat]];
            }
            //Half star
            else if (rating > i) {
                [starString appendAttributedString:[[NSAttributedString alloc]
                                                    initWithString:NSLocalizedString(@"􀋄 ", nil) attributes:activeStarFormat]];
            }
            // Grey star
            else {
                [starString appendAttributedString:[[NSAttributedString alloc]
                                                    initWithString:NSLocalizedString(@"􀋂 ", nil) attributes:inactiveStarFormat]];
            }
        }
    }
    return starString;
}

- (NSString *) unitStringFromBytes:(CGFloat)bytes {
    static const char units[] = { '\0', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };
    static int maxUnits = sizeof units - 1;
    
    int multiplier = 1000;
    int exponent = 0;
    
    while (bytes >= multiplier && exponent < maxUnits) {
        bytes /= multiplier;
        exponent++;
    }
    NSNumberFormatter* formatter = [NSNumberFormatter new];
    
    NSString *unitString = [NSString stringWithFormat:@"%cB", units[exponent]];
    if ([unitString isEqualToString:@"kB"])
        unitString = @"K";
    
    return [NSString stringWithFormat:@"%@ %@", [formatter stringFromNumber: [NSNumber numberWithInt: round(bytes)]], unitString];
}

- (void)addFileInfo:(NSURL *)url {
    if (_addedFileInfo)
        return;
    _addedFileInfo = YES;
    NSMutableDictionary *attrDict = [NSMutableDictionary new];
    attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
    NSError *error = nil;
    NSDictionary * fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:&error];
    if (fileAttributes) {
        NSDate *modificationDate = (NSDate *)fileAttributes[NSFileModificationDate];
        [self addInfoLine:[NSString stringWithFormat:@"Last modified: %@", [self formattedStringFromDate:  modificationDate]] attributes:attrDict linebreak:YES];
        NSInteger fileSize = ((NSNumber *)fileAttributes[NSFileSize]).integerValue;
        [self addInfoLine:[self unitStringFromBytes:fileSize] attributes:attrDict linebreak:YES];
    } else {
        NSLog(@"Could not read file attributes!");
    }
}


- (NSString *)formattedStringFromDate:(NSDate *)date {
    
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.doesRelativeDateFormatting = YES;
    
    if([date isToday]) {
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    }
    else if ([date isYesterday]) {
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    }
    else if ([date isThisWeek]) {
        NSDateComponents *weekdayComponents = [[NSCalendar currentCalendar] components:(NSCalendarUnit)kCFCalendarUnitWeekday fromDate:date];
        NSUInteger weekday = (NSUInteger)weekdayComponents.weekday;
        NSString *weekdayString = (dateFormatter.weekdaySymbols)[weekday - 1];
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterNoStyle;
        return [NSString stringWithFormat:@"%@ at %@", weekdayString, [dateFormatter stringFromDate:date]];
    } else {
        dateFormatter.doesRelativeDateFormatting = NO;
        dateFormatter.dateFormat = @"d MMM yyyy HH.mm";
    }
    
    return [dateFormatter stringFromDate:date];
}

- (NSMutableDictionary *)metadataFromURL:(NSURL *)url {
    NSMutableDictionary *metaDict = [NSMutableDictionary new];
    if (![Blorb isBlorbURL:url])
        return nil;
    
    Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
    metaDict[@"cover"] = [blorb coverImageData];
    
    metaDict[@"IFhd"] = [blorb ifidFromIFhd];
    if (metaDict[@"IFhd"]) {
        metaDict[@"IFhdTitle"] = [self titleFromIfid:metaDict[@"IFhd"]];
    } else NSLog(@"No IFdh resource in Blorb file");
    
    for (NSString *chunkName in blorb.optionalChunks.allKeys) {
        metaDict[chunkName] = blorb.optionalChunks[chunkName];
        NSLog(@"metaDict[%@] = \"%@\"", chunkName, metaDict[chunkName]);
    }
    
    NSData *data = blorb.metaData;
    if (data) {
        NSError *error = nil;
        NSXMLDocument *xml =
        [[NSXMLDocument alloc] initWithData:data
                                    options:NSXMLDocumentTidyXML
                                      error:&error];
        if (error)
            NSLog(@"Error: %@", error);
        
        NSArray *nodeNames = @[@"title", @"author", @"headline", @"description"];
        
        NSXMLElement *story =
        [[xml rootElement] elementsForName:@"story"].firstObject;
        if (story) {
            NSXMLElement *idElement = [story elementsForName:@"bibliographic"].firstObject;
            for (NSXMLNode *node in idElement.children) {
                if (node.stringValue.length) {
                    for (NSString *nodeName in nodeNames) {
                        if ([node.name compare:nodeName] == 0) {
                            if ([nodeName isEqualToString:@"description"])
                                metaDict[@"blurb"] = node.stringValue;
                            else
                                metaDict[nodeName] = node.stringValue;
                            break;
                        }
                    }
                }
            }
            if (_ifid) {
                metaDict[@"ifid"] = _ifid;
            } else {
                idElement = [story elementsForName:@"identification"].firstObject;
                for (NSXMLNode *node in idElement.children) {
                    if ([node.name compare:@"ifid"] == 0 && node.stringValue.length) {
                        metaDict[@"ifid"] = node.stringValue;
                        break;
                    }
                }
            }
        }
    }
    return metaDict;
}

- (NSString *)titleFromIfid:(NSString *)ifid {
    if (!ifid)
        return nil;
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"Could not create new context!");
        return nil;
    }
    __block Game *game = nil;
    
    [context performBlockAndWait:^{
        NSError *error = nil;
        NSArray *fetchedObjects;
        
        NSFetchRequest *fetchRequest = [NSFetchRequest new];
        
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@", ifid];
        
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects && fetchedObjects.count) {
            if (fetchedObjects.count > 1)
                NSLog(@"Found %ld games with ifid %@", fetchedObjects.count, ifid);
            game = fetchedObjects.firstObject;
        }
    }];
    
    if (game) {
        if (game.metadata.cover.data) {
            NSImage *image = [[NSImage alloc] initWithData:(NSData *)game.metadata.cover.data];
            if (image) {
                _imageView.image = image;
                _imageView.accessibilityLabel = game.metadata.cover.imageDescription;
            }
        }
        return game.metadata.title;
    } else {
        return nil;
    }
}

- (NSString *)titleFromHash:(NSString *)hash {
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"context is nil!");
        return nil;
    }
    __block Game *game = nil;
    
    [context performBlockAndWait:^{
        NSError *error = nil;
        NSArray *fetchedObjects;
        
        NSFetchRequest *fetchRequest = [NSFetchRequest new];
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"hashTag like[c] %@", hash];
        
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects && fetchedObjects.count) {
            if (fetchedObjects.count > 1)
                NSLog(@"Found %ld games with requested hash", fetchedObjects.count);
            game = fetchedObjects.firstObject;
        }
    }];
    if (game) {
        if (game.metadata.cover.data) {
            NSImage *image = [[NSImage alloc] initWithData:(NSData *)game.metadata.cover.data];
            if (image) {
                _imageView.image = image;
                _imageView.accessibilityLabel = game.metadata.cover.imageDescription;
            }
        }
        return game.metadata.title;
    } else {
        return nil;
    }
}

- (NSString *)ifidFromFile:(NSString *)path {
    void *context = get_babel_ctx();
    char *format = babel_init_ctx((char*)path.UTF8String, context);
    if (!format || !babel_get_authoritative_ctx(context))
    {
        babel_release_ctx(context);
        return nil;
    }
    
    char buf[TREATY_MINIMUM_EXTENT];
    
    int rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, context);
    if (rv <= 0)
    {
        babel_release_ctx(context);
        return nil;
    }
    
    babel_release_ctx(context);
    return @(buf);
}

- (NSString *)titleFromSave:(NSString *)path {
    NSString *title = nil;
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (data) {
        title = [self titleFromQuetzalSave:data];
        if (!title.length) {
            NSString *hash = [self hashFromQuetzalSave:data];
            if (hash.length) {
                title = [self titleFromHash:hash];
            }
        }
    }
    return title;
}

- (NSString *)titleFromDataFile:(NSString *)path {
    NSString *title = nil;
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (data) {
        NSString *ifid = [self ifidFromData:data];
        if (ifid.length) {
            title = [self titleFromIfid:ifid];
        }
        if (!title.length) {
            title = [self titleFromQuetzalSave:data];
        }
        if (!title.length) {
            title = ifid;
        }
    }
    return title;
}

- (NSString *)titleFromQuetzalSave:(NSData *)data {
    NSString *title = nil;
    NSString *ifid = nil;
    if (data) {
        const void *ptr = data.bytes;
        if (isForm(ptr, 'I', 'F', 'Z', 'S') || isForm(ptr, 'B', 'F', 'Z', 'S')) {
            ptr += 12;
            unsigned int chunkID;
            NSUInteger length = chunkIDAndLength(ptr, &chunkID);
            if (chunkID == IFFID('I', 'F', 'h', 'd')) {
                // Game Identifier Chunk
                NSUInteger releaseNumber = unpackShort(ptr + 8);
                
                NSData *serialData = [NSData dataWithBytes:ptr + 10 length:6];
                NSUInteger checksum  = unpackShort(ptr + 16);
                NSString *serialNum = [[NSString alloc] initWithData:serialData encoding:NSASCIIStringEncoding];
                
                // See if we can find a path to the game file as well
                __block NSString *path = nil;
                ptr += paddedLength(length) + 8;
                length = chunkIDAndLength(ptr, &chunkID);
                if (chunkID == IFFID('I', 'n', 't', 'D')) {
                    // Found IntD chunk!
                    NSData *pathData = [NSData dataWithBytes:ptr + 20 length:length - 12];
                    path = [[NSString alloc] initWithData:pathData encoding:NSASCIIStringEncoding];
                }
                
                NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
                if (context) {
                    __block Game *game = nil;
                    
                    [context performBlockAndWait:^{
                        NSError *error = nil;
                        NSArray *fetchedObjects;
                        
                        NSFetchRequest *fetchRequest = [NSFetchRequest new];
                        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
                        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"serialString LIKE[c] %@ AND releaseNumber == %d AND checksum == %d", serialNum, releaseNumber, checksum];
                        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
                        
                        //If no match, look for path
                        if (!fetchedObjects.count && path.length) {
                            fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path LIKE[c] %@", path];
                            fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
                        }
                        
                        if (fetchedObjects.count > 1)
                            NSLog(@"Found %ld matching games!", fetchedObjects.count);
                        game = fetchedObjects.firstObject;
                        
                    }];
                    
                    if (game) {
                        title = game.metadata.title;
                        NSImage *image = [[NSImage alloc] initWithData:(NSData *)game.metadata.cover.data];
                        if (image) {
                            _imageView.image = image;
                            _imageView.accessibilityLabel = game.metadata.cover.imageDescription;
                        }
                    }
                }
                
                if (!title) {
                    if (path.length) {
                        title = path.lastPathComponent;
                    } else if (!ifid) {
                        serialNum = [serialNum stringByReplacingOccurrencesOfString:@"[^0-Z]" withString:@"-" options:NSRegularExpressionSearch range:NSMakeRange(0, 6)];
                        
                        NSInteger date = serialNum.intValue;
                        
                        if ((date < 700000 || date > 900000) && date != 0 && date != 999999) {
                            ifid = [NSString stringWithFormat:@"ZCODE-%ld-%@-%04lX", releaseNumber, serialNum, (unsigned long)checksum];
                        } else {
                            ifid = [NSString stringWithFormat:@"ZCODE-%ld-%@", releaseNumber, serialNum];
                        }
                        title = [self titleFromIfid:ifid];
                    }
                }
            }
        }
    } else NSLog(@"No IFZS chunk found in save file!");
    if (!title.length && ![ifid hasPrefix:@"ZCODE-18284-------"]) {
        title = ifid;
    }
    return title;
}

- (NSString *)hashFromQuetzalSave:(NSData *)data {
    NSMutableString *hexString = nil;
    if (data.length >= 84) {
        const void *ptr = data.bytes;
        if (isForm(ptr, 'I', 'F', 'Z', 'S')) {
            ptr += 12;
            unsigned int chunkID;
            chunkIDAndLength(ptr, &chunkID);
            if (chunkID == IFFID('I', 'F', 'h', 'd')) {
                // Game Identifier Chunk
                Byte *bytes64 = (Byte *)malloc(64);
                [data getBytes:bytes64
                         range:NSMakeRange(20, 64)];
                
                hexString = [NSMutableString stringWithCapacity:(64 * 2)];
                
                for (int i = 0; i < 64; ++i) {
                    [hexString appendFormat:@"%02x", (unsigned int)bytes64[i]];
                }
                free(bytes64);
            }
        }
    }
    return hexString;
}

- (NSString *)ifidFromData:(NSData *)data {
    NSString *ifid = nil;
    if (data.length > 12) {
        NSRange headerRange = NSMakeRange(0, MIN(data.length, 69));
        NSString *header = [[NSString alloc] initWithData:[data subdataWithRange:headerRange] encoding:NSASCIIStringEncoding];
        if ([header hasPrefix:@"* //"]) {
            header = [header substringFromIndex:4];
            NSRange firstSlash = [header rangeOfString:@"//"];
            if (firstSlash.location != NSNotFound) {
                ifid = [header substringToIndex:firstSlash.location];
            }
        }
    }
    return ifid;
}

@end


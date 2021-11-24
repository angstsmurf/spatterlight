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

#import "NSData+Categories.h"

#import "iFictionPreviewController.h"
#import "PreviewViewController.h"
#import "InfoView.h"
#import "NonInterpolatedImage.h"

#include "babel_handler.h"

@interface PreviewViewController () <QLPreviewingController>
{
    BOOL iFiction;
    BOOL alreadySetupHorizontal;
}

@property (weak) IBOutlet NSScrollView *largeScrollView;
@property (weak) IBOutlet NSView *backgroundView;
@property InfoView *verticalView;
@property NSView *horizontalView;
@property NSMutableDictionary *metaDict;

@end

@implementation PreviewViewController

+ (NSImage *)iconFromURL:(NSURL *)url {
    NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
    NSData *imageData = icon.TIFFRepresentation;
    if ([[imageData sha256String] isEqualToString:@"4D5665BE8E0382BFFB7500F86A69117080A036B9698DC2395F3A86F9DCE7170E"]) {
        return [NSImage imageNamed:@"house"];
    }
    return icon;
}

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
            size = NSMakeSize(565, 285);
        NSRect frame = _largeScrollView.frame;
        frame.size = size;
        _largeScrollView.frame = frame;
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

- (void)preparePreviewOfSearchableItemWithIdentifier:(NSString *)identifier queryString:(NSString *)queryString completionHandler:(void (^)(NSError * _Nullable))handler {
    //    NSLog(@"preparePreviewOfSearchableItemWithIdentifier: %@ queryString: %@", identifier, queryString );
    _ifid = nil;

    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSError *contexterror = [NSError errorWithDomain:NSCocoaErrorDomain code:6 userInfo:@{
            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Could not create new background context\n"]}];
        NSLog(@"Could not create new context!");
        handler(contexterror);
        return;
    }

    NSURL __block *url = nil;
    
    Game __block *game = nil;
    Metadata __block *metadata = nil;
    Image __block *image = nil;
    
    BOOL __block giveUp = NO;

    NSError __block *error;
    
    [context performBlockAndWait:^{
        
        error = [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:@{
            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"This game has been deleted from the Spatterlight database.\n"]}];
        
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
            if (!metadata) {
                NSLog(@"Image has no metadata!");
                giveUp = YES;
            }
            if (!game)
                NSLog(@"Image has no game!");
            if (!image.data) {
                NSLog(@"Image has no data!");
                giveUp = YES;
            }
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
        handler(error);
        return;
    }
    
    if (image && !image.data) {
        error = [NSError errorWithDomain:NSCocoaErrorDomain code:102 userInfo:@{
            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Image has no data!?!\n"]}];
        handler(error);
        return;
    }
    
    if (!metadata) {
        //        error = [NSError errorWithDomain:NSCocoaErrorDomain code:101 userInfo:@{
        //            NSLocalizedDescriptionKey: [NSString stringWithFormat:@"No metadata!?!\n"]}];
        handler(error);
        return;
    }

    InfoView *infoView = [[InfoView alloc] initWithFrame:_largeScrollView.bounds];

    if (game)
        url = [game urlForBookmark];
    
    if (!metadata.cover.data && metadata.blurb.length == 0 && metadata.author.length == 0 && metadata.headline.length == 0 && url) {
        Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
        if (blorb)
            infoView.imageData = [blorb coverImageData];
        if (!infoView.imageData) {
            _showingIcon = YES;
            NSImage *icon = [PreviewViewController iconFromURL:url];
            infoView.imageData = icon.TIFFRepresentation;
        }
    }
    
    _largeScrollView.documentView = infoView;
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

    NSString *extension = url.pathExtension.lowercaseString;
    
    if ([extension isEqualToString:@"ifiction"]) {
        iFiction = YES;
        [_imageView removeFromSuperview];

        NSScrollView *scrollView = _textview.enclosingScrollView;
        [scrollView removeFromSuperview];
        [_largeScrollView removeFromSuperview];

        scrollView.translatesAutoresizingMaskIntoConstraints = YES;
        [self.view addSubview:scrollView];

        iFictionPreviewController *iFictionController = [iFictionPreviewController new];
        iFictionController.textview  = _textview;

        scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scrollView.frame = self.view.bounds;
        _textview.frame = self.view.bounds;

        if (@available(macOS 11, *)) {
            self.preferredContentSize = NSMakeSize(820, 846);
        }
        
        [iFictionController preparePreviewOfFileAtURL:url completionHandler:handler];
        return;
    }

    _horizontalView = _backgroundView;

    if ([extension isEqualToString:@"glkdata"] || [extension isEqualToString:@"glksave"] || [extension isEqualToString:@"qut"] || [extension isEqualToString:@"d$$"]) {
        [self noPreviewForURL:url handler:handler];
        return;
    }
    
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"Could not create new context! Bailing to noPreviewForURL");
        [self noPreviewForURL:url handler:handler];
        return;
    }
    
    NSMutableDictionary __block *metadata = nil;
    
    PreviewViewController __weak *weakSelf = self;

    BOOL __block giveUp = NO;

    [context performBlockAndWait:^{
        PreviewViewController *strongSelf = weakSelf;
        NSError *error = nil;
        NSArray *fetchedObjects;
        
        NSFetchRequest *fetchRequest = [NSFetchRequest new];
        
        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path like[c] %@", url.path];
        
        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects == nil) {
            NSLog(@"QuickLook: fetch request failed. %@ Bailing to noPreviewForURL",error);
            [strongSelf noPreviewForURL:url handler:handler];
            giveUp = YES;
            return;
        }
        if (fetchedObjects.count == 0) {
            strongSelf.ifid = [strongSelf ifidFromFile:url.path];
            if (strongSelf.ifid) {
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@", strongSelf.ifid];
                fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
            }
            if (fetchedObjects.count == 0) {
                metadata = [strongSelf metadataFromBlorb:url];
                if (metadata == nil || metadata.count == 0) {
                    // Found no matching path or ifid. File not a blorb or blorb empty. Bailing to noPreviewForURL
                    [strongSelf noPreviewForURL:url handler:handler];
                    giveUp = YES;
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
            
            _metaDict = [[NSMutableDictionary alloc] initWithCapacity:attributes.count];
            
            for (NSString *attr in attributes) {
                [_metaDict setValue:[meta valueForKey:attr] forKey:attr];
            }
            _metaDict[@"ifid"] = game.ifid;
            _metaDict[@"cover"] = game.metadata.cover.data;
            _metaDict[@"lastPlayed"] = game.lastPlayed;
        } else {
            _metaDict = metadata;
        }
        
        if (!_metaDict.count) {
            NSLog(@"QuickLook: Could not retrieve any metadata from Game managed object. Bailing to noPreviewForURL");
            [strongSelf noPreviewForURL:url handler:handler];
            giveUp = YES;
        }
    }];

    if (giveUp)
        return;

    if (!_metaDict[@"title"])
        _metaDict[@"title"] = url.lastPathComponent;
    
    if (_metaDict[@"cover"]) {
        [_imageView addImageFromData:(NSData *)_metaDict[@"cover"]];
        _imageView.accessibilityLabel = _metaDict[@"coverArtDescription"];
    } else if ([Blorb isBlorbURL:url]) {
        Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
        [_imageView addImageFromData:[blorb coverImageData]];
    }
    
    [self updateWithMetadata:_metaDict url:url];
    if (_metaDict.count <= 2 && url) {
        [self addFileInfo:url];
    }

    if (!_imageView.hasImage) {
        _showingIcon = YES;
        NSImage *image = [PreviewViewController iconFromURL:url];
        if (image == [NSImage imageNamed:@"house"])
            _showingIcon = NO;
        NSData *imageData = image.TIFFRepresentation;
        _metaDict[@"cover"] = imageData;
        // If we add the icon image directly it becomes low resolution and blurry
        [_imageView addImageFromData:imageData];
    }

    [self finalAdjustments:handler];
}

#pragma mark Layout

- (void)drawVertical {
    if(!_vertical)
        return;
    _verticalView = [[InfoView alloc] initWithFrame:_largeScrollView.frame];
    _largeScrollView.documentView = _verticalView;
    [_verticalView updateWithDictionary:_metaDict];
}

- (void)drawHorizontal {
    _largeScrollView.documentView = _horizontalView;
    _horizontalView.frame = _largeScrollView.frame;
    [self setUpHorizontalView];
}

-(void)finalAdjustments:(void (^)(NSError * _Nullable))handler  {
    NSScrollView *scrollView = _textview.enclosingScrollView;
    NSView *superView = _imageView.superview;
    NSSize __block viewSize = superView.frame.size;
    PreviewViewController __weak *weakSelf = self;
    
    double delayInSeconds = 0.1;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        PreviewViewController *strongSelf = weakSelf;
        if (strongSelf) {
            viewSize = superView.frame.size;
            strongSelf.vertical = (viewSize.width - viewSize.height <= 20);
            if (strongSelf.vertical) {
                [strongSelf drawVertical];
                strongSelf.showingView = YES;
                handler(nil);
            } else {
                [strongSelf drawHorizontal];

                CGFloat scrollheight = scrollView.frame.size.height;
                CGFloat textheight = strongSelf.textview.frame.size.height;
                CGFloat viewheight = superView.frame.size.height;
                [scrollView.contentView scrollToPoint:NSZeroPoint];
                if (textheight < viewheight - 40 && scrollheight < textheight) {
                    NSRect frame = strongSelf.textview.enclosingScrollView.frame;
                    //Text is mysteriously cropped at the bottom
                    CGFloat diff = ceil((textheight - scrollheight) / 2);
                    frame.size.height = textheight;
                    frame.origin.y -= diff;
                    scrollView.frame = frame;
                    scrollView.contentView.frame = scrollView.bounds;
                }
                strongSelf.showingView = YES;
                handler(nil);
            }
        }
    });
}

-(void)viewWillLayout {
    if (!_showingView || iFiction)
        return;
    NSSize viewSize = self.view.frame.size;
    _vertical = (viewSize.width - viewSize.height <= 20);
    if (_vertical && (_largeScrollView.documentView != _verticalView ||  NSWidth(self.view.frame) < NSWidth(_verticalView.frame))) {
        // This .1 second delay prevents the tall and narrow view from sometimes
        // getting the wrong width when added during live resize
        PreviewViewController __weak *weakSelf = self;
        double delayInSeconds = 0.1;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            PreviewViewController *strongSelf = weakSelf;
            if (strongSelf) {
                [strongSelf drawVertical];
            }
        });
    } else if (!_vertical) {
        if (_largeScrollView.documentView != _horizontalView)
            [self drawHorizontal];
    }
}

- (void)viewDidLayout {
    if (!_showingView || iFiction || _vertical)
        return;
    NSSize viewSize = self.view.frame.size;
    NSScrollView *textScrollView = _textview.enclosingScrollView;

    CGFloat textHeight = [self heightForString:_textview.textStorage andWidth:NSWidth(textScrollView.frame)];
    if (textHeight <= viewSize.height - 40) {
        _textClipHeight.constant = textHeight;
    } else {
        _textClipHeight.constant = viewSize.height - 40;
    }

    if (NSHeight(_imageView.frame) < NSHeight(textScrollView.frame)) {
        _imageTopEqualsTextTop.active = YES;
        _imageCenterYtoContainerCenter.active = YES;
        _imageCenterYtoContainerCenter.priority = 250;
    } else {
        _imageTopEqualsTextTop.active = NO;
        _imageCenterYtoContainerCenter.active = YES;
        _imageCenterYtoContainerCenter.priority = 1000;
    }
}

- (void)imageShadow {
    NSShadow *shadow = [NSShadow new];
    shadow.shadowOffset = NSMakeSize(2, -2);
    shadow.shadowColor = [NSColor controlShadowColor];
    shadow.shadowBlurRadius = 2;
    NonInterpolatedImage *imageView = _imageView;
    imageView.wantsLayer = YES;
    imageView.superview.wantsLayer = YES;
    imageView.shadow = shadow;
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
    return ceil(proposedRect.size.height + 20);
}

//- (void)debugPrintFinalLayout{
//    NSLog(@"Final layout: view frame: %@ subview frame:%@ image view frame %@ scroll view frame:%@ text view frame:%@", NSStringFromRect(self.view.frame), NSStringFromRect(_imageView.superview.frame), NSStringFromRect(_imageView.frame), NSStringFromRect(_textview.enclosingScrollView.frame), NSStringFromRect(_textview.frame));
//    NSLog(@"Layout is %@", _vertical ? @"vertical" : @"horizontal");
//    NSLog(@"Showing icon: %@", _showingIcon ? @"YES" : @"NO");
//    NSLog(@"_textview.string.length:%ld", _textview.string.length);
//}

- (void)setUpHorizontalView {
    NSView *superView = [InfoView addTopConstraintsToView:_backgroundView];

    [superView addConstraint:
     [NSLayoutConstraint constraintWithItem:_backgroundView
                                  attribute:NSLayoutAttributeBottom
                                  relatedBy:NSLayoutRelationEqual
                                     toItem:superView
                                  attribute:NSLayoutAttributeBottom
                                 multiplier:1.0
                                   constant:0]];

    [_textview.enclosingScrollView.contentView scrollToPoint:NSZeroPoint];

    if (alreadySetupHorizontal) {
        return;
    }
    alreadySetupHorizontal = YES;

    NSSize imageSize = _imageView.originalImageSize;
    CGFloat ratio = _imageView.ratio;

    CGFloat containerWidth = NSWidth(_backgroundView.frame);
    CGFloat containerHeight = NSHeight(_backgroundView.frame);

    CGFloat newHeight, newWidth;
    NSRect newImageFrame, newTextFrame;

    _imageHeightTracksImageWidth =
    [NSLayoutConstraint constraintWithItem:_imageView
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:_imageView
                                 attribute:NSLayoutAttributeWidth
                                multiplier:ratio
                                  constant:0];

    [_backgroundView addConstraint:_imageHeightTracksImageWidth];

    if (imageSize.height >= imageSize.width) {
        // Image is narrow
        newHeight = containerHeight - 40;
        newWidth = newHeight / ratio;

        newImageFrame = _imageView.frame;
        newImageFrame.size = NSMakeSize(newWidth, newWidth);
        _imageView.frame = newImageFrame;
    } else {
        // Image is wide
        newWidth = containerWidth * 0.5 - 30;
        newHeight = newWidth * ratio;
        if (newHeight > containerHeight - 40) {
            newHeight = containerHeight - 40;
            newWidth = newHeight / ratio;
        }

        newImageFrame = _imageView.frame;
        newImageFrame.size = NSMakeSize(newWidth, newHeight);
        _imageView.frame = newImageFrame;
    }

    NSScrollView *textScrollView = _textview.enclosingScrollView;

    CGFloat textHeight = [self heightForString:_textview.textStorage andWidth:NSWidth(textScrollView.frame)];

    _textClipHeight =
    [NSLayoutConstraint constraintWithItem:textScrollView
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:nil
                                 attribute:NSLayoutAttributeNotAnAttribute
                                multiplier:1
                                  constant:NSHeight(textScrollView.frame)];
    _textClipHeight.priority = 900;
    [_backgroundView addConstraint:_textClipHeight];

    newTextFrame = textScrollView.frame;
    newTextFrame.size.width = containerWidth - NSWidth(newImageFrame) - 60;
    newTextFrame.size.height = MIN(textHeight, containerHeight - 40);
    newTextFrame.origin.x = NSMaxX(newTextFrame) + 20;
    newTextFrame.origin.y = ceil((containerHeight - NSHeight(newTextFrame)) / 2);
    textScrollView.frame = newTextFrame;

    if (textHeight <= containerHeight - 40) {
        // All text fits on screen. Disable text scrollview top and bottom constraints
        // And set a new fixed height
        _textTopToContainerTop.active = NO;
        _textBottomToContainerBottom.active = NO;

        _textClipHeight.constant = textHeight;
        _textview.frame = textScrollView.bounds;
    } else {
        // Text does not fit on screen. Set text scrollview height to container height
        _textTopToContainerTop.active = YES;
        _textBottomToContainerBottom.active = YES;
        _textClipHeight.constant = containerHeight - 40;
    }

    _imageTopEqualsTextTop =
    [NSLayoutConstraint constraintWithItem:_imageView
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:_textview
                                 attribute:NSLayoutAttributeTop
                                multiplier:1.0
                                  constant:0];
    _imageTopEqualsTextTop.active = NO;

    [_backgroundView addConstraint:_imageTopEqualsTextTop];
    _textview.frame = _textview.enclosingScrollView.bounds;

    if (NSHeight(_imageView.frame) <= NSHeight(textScrollView.frame)) {
        // Image height is less than text height
        // Pinning image top to text top
        _imageTopEqualsTextTop.active = YES;
    } else {
        //Image height is greater than text height
        //Centering image vertically
        _imageTopEqualsTextTop.active = NO;
    }

    if (!_showingIcon)
        [self imageShadow];
    else
        _textLeadingTracksImageTrailing.constant = 0;
}

#pragma mark Adding metadata

- (void)noPreviewForURL:(NSURL *)url handler:(void (^)(NSError *))handler {
    NSFont *systemFont = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
    NSMutableDictionary *attrDict = [NSMutableDictionary new];
    attrDict[NSFontAttributeName] = systemFont;
    attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
    if (!_metaDict)
        _metaDict = [NSMutableDictionary new];
    _metaDict[@"title"] = url.lastPathComponent;
    [self addInfoLine:url.lastPathComponent attributes:attrDict linebreak:NO];

    attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];

    NSString *description;

    if ([url.pathExtension.lowercaseString isEqualToString:@"d$$"]) {
        description = @"Possibly an AGT game. Try opening with Spatterlight to convert to AGX format.";
    } else if ([url.path.pathExtension.lowercaseString isEqualToString:@"glksave"] || [url.pathExtension.lowercaseString isEqualToString:@"qut"]) {
        NSString *saveTitle = [self titleFromSave:url.path];
        if (saveTitle.length)
            description = [NSString stringWithFormat:@"Save file associated with the game %@.", saveTitle];
        else {
            description = @"Save file for an unknown game.";
        }
    } else if ([url.pathExtension.lowercaseString isEqualToString:@"glkdata"]) {
        NSString *dataTitle = [self titleFromDataFile:url.path];
        if (dataTitle.length)
            description = [NSString stringWithFormat:@"Data file associated with the game %@.", dataTitle];
        else
            description = @"Data file for an unknown game.";
    }

    if (description.length) {
        [self addInfoLine:description attributes:attrDict linebreak:YES];
        _metaDict[@"blurb"] = description;
    }

    if (_ifid.length) {
        [self addInfoLine:[@"IFID: " stringByAppendingString:_ifid] attributes:attrDict linebreak:YES];
        _metaDict[@"ifid"] = _ifid;
    }

    [self addFileInfo:url];

    if (!_imageView.hasImage) {
        _showingIcon = YES;
        NSImage *image = [PreviewViewController iconFromURL:url];
        if (image == [NSImage imageNamed:@"house"])
            _showingIcon = NO;
        NSData *imageData = image.TIFFRepresentation;
        _metaDict[@"cover"] = imageData;
        [_imageView addImageFromData:imageData];
    }

    [self finalAdjustments:handler];
}

- (void)updateWithMetadata:(NSDictionary *)metadict url:(nullable NSURL *)url {
    if (metadict && metadict.count) {
        NSFont *systemFont = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
        NSMutableDictionary *attrDict = [NSMutableDictionary new];
        attrDict[NSFontAttributeName] = systemFont;
        attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
        [self addInfoLine:metadict[@"title"] attributes:attrDict linebreak:NO];
        
        if (!metadict[@"title"]) {
            [self addInfoLine:url.lastPathComponent attributes:attrDict linebreak:NO];
            
            if (metadict[@"SNam"]) {
                NSMutableDictionary *metamuta = metadict.mutableCopy;
                metamuta[@"IFhd"] = @"dummyIFID";
                metamuta[@"IFhdTitle"] = metadict[@"SNam"];
                metadict = metamuta;
                //                NSLog(@"Set IFhdTitle to %@", metadict[@"SNam"]);
            }
            
            if (metadict[@"IFhd"]) {
                attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
                NSString *resBlorbStr = @"Resource associated with the game\n";
                if (metadict[@"IFhdTitle"])
                    resBlorbStr = [resBlorbStr stringByAppendingString:metadict[@"IFhdTitle"]];
                else
                    resBlorbStr = [resBlorbStr stringByAppendingString:metadict[@"IFhd"]];
                [self addInfoLine:resBlorbStr attributes:attrDict linebreak:YES];
                _metaDict[@"title"] = resBlorbStr;
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
        NSString *lastPlayedString = [InfoView formattedStringFromDate:lastPlayed];
        // It looks better to have the "last played" line come last if we are showing file info,
        // so that modification date follows last played date and the two dates are grouped.
        if (lastPlayed && !noMeta) {
            [self addInfoLine:[NSString stringWithFormat:@"Last played: %@", lastPlayedString] attributes:attrDict linebreak:YES];
        }
        
        if (!noMeta) {
            attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
        } else {
            attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        }
        if (metadict[@"ifid"])
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
    NSAttributedString *starString = [InfoView starString:rating alignment:NSTextAlignmentLeft];
    [_textview.textStorage appendAttributedString:starString];
}

+ (NSString *)unitStringFromBytes:(CGFloat)bytes {
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
        [self addInfoLine:[NSString stringWithFormat:@"Last modified: %@", [InfoView formattedStringFromDate:modificationDate]] attributes:attrDict linebreak:YES];

        NSInteger fileSize = ((NSNumber *)fileAttributes[NSFileSize]).integerValue;
        NSString *fileSizeString = [PreviewViewController unitStringFromBytes:fileSize];
        _metaDict[@"fileSize"] = fileSizeString;
        [self addInfoLine:fileSizeString attributes:attrDict linebreak:YES];

        _metaDict[@"modificationDate"] = modificationDate;
        _metaDict[@"creationDate"] = (NSDate *)fileAttributes[NSFileCreationDate];
        _metaDict[@"lastOpenedDate"] = [PreviewViewController lastOpenedDateFromURL:url];

        error = nil;
        NSString *value;
        [url getResourceValue:&value forKey:NSURLLocalizedTypeDescriptionKey error:&error];
        _metaDict[@"fileType"] = value;
    } else {
        NSLog(@"Could not read file attributes! %@", error);
    }
}

+ (NSDate *)lastOpenedDateFromURL:(NSURL *)url {
    CFURLRef cfurl = CFBridgingRetain(url);
    MDItemRef mdItem = MDItemCreateWithURL(NULL, cfurl);
    CFTypeRef dateRef = MDItemCopyAttribute(mdItem, kMDItemLastUsedDate);
    NSDate *date = CFBridgingRelease(dateRef);
    CFRelease(cfurl);
    return date;
}

- (NSMutableDictionary *)metadataFromBlorb:(NSURL *)url {
    if (![Blorb isBlorbURL:url])
        return nil;

    NSMutableDictionary *metaDict = [NSMutableDictionary new];
    
    Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
    metaDict[@"cover"] = [blorb coverImageData];
    
    metaDict[@"IFhd"] = [blorb ifidFromIFhd];
    if (metaDict[@"IFhd"]) {
        metaDict[@"IFhdTitle"] = [self titleFromIfid:metaDict[@"IFhd"]];
    } else NSLog(@"No IFdh resource in Blorb file");
    
    for (NSString *chunkName in blorb.optionalChunks.allKeys) {
        metaDict[chunkName] = blorb.optionalChunks[chunkName];
        //        NSLog(@"metaDict[%@] = \"%@\"", chunkName, metaDict[chunkName]);
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
    Game __block *game = nil;
    
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
        [self addImageFromGame:game];
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
    Game __block *game = nil;
    
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
        [self addImageFromGame:game];
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
        NSString *ifid = [PreviewViewController ifidFromData:data];
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
                NSString __block *path = nil;
                ptr += paddedLength(length) + 8;
                length = chunkIDAndLength(ptr, &chunkID);
                if (chunkID == IFFID('I', 'n', 't', 'D')) {
                    // Found IntD chunk!
                    NSData *pathData = [NSData dataWithBytes:ptr + 20 length:length - 12];
                    path = [[NSString alloc] initWithData:pathData encoding:NSASCIIStringEncoding];
                }
                
                NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
                if (context) {
                    Game __block *game = nil;
                    
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
                        [self addImageFromGame:game];
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

+ (NSString *)ifidFromData:(NSData *)data {
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

- (void)addImageFromGame:(Game *)game {
    if (game.metadata.cover.data) {
        [_imageView addImageFromManagedObject:game.metadata.cover];
        if (_imageView.hasImage) {
            if (!_metaDict)
                _metaDict = [NSMutableDictionary new];
            _metaDict[@"cover"] = (NSData *)game.metadata.cover.data;
            _metaDict[@"coverArtDescription"] = game.metadata.cover.imageDescription;;
        }
    }
}

@end


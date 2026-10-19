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
#import "Image.h"
#import "Metadata.h"

#import "FileSignature.h"
#import "NSString+Categories.h"
#import "ReadOnlyStore.h"
#import "IFictionMetadata.h"
#import "IFStory.h"
#import "IFIdentification.h"
#import "IFBibliographic.h"

#import "iFictionPreviewController.h"
#import "PreviewViewController.h"
#import "InfoView.h"
#import "NonInterpolatedImage.h"

#include "babel_handler.h"

@interface PreviewViewController () <QLPreviewingController> {
    BOOL iFiction;
    BOOL imageIsNarrow;
    NSData *imageData;
}

@property (strong) IBOutlet NSScrollView *largeScrollView;
@property (strong) IBOutlet NSView *horizontalView;
@property InfoView *verticalView;
@property NSMutableDictionary *metaDict;
@property BOOL changing;
@property CGFloat forcedHeightForWidth;
@property CGFloat changedMyMindForWidth;

@end

@implementation PreviewViewController

// App-level categories are only available if their .m file is compiled into
// this extension target. A missing one otherwise shows up as an unrecognized
// selector deep inside a preview (see the 1.4.6 -[NSData sha256String] crash;
// hashing now goes through FileSignature, which BlorbFramework provides), so
// check the ones we still use at launch. If this fires, add the named file to
// the SpatterlightQuickLook target's Compile Sources build phase.
+ (void)initialize {
    if (self != [PreviewViewController class])
        return;

    [ReadOnlyStore checkCategoryMethods:@[
        @[NSString.class, @"signatureFromFile", @"NSString+Categories.m"],
        @[NSFont.class, @"fontToFitWidth:sampleText:", @"NSFont+Categories.m"]
    ] target:@"SpatterlightQuickLook"];
}

// Bold, 20 points, for the title line.
+ (NSMutableDictionary *)titleAttributes {
    return [self attributesWithFont:[NSFont systemFontOfSize:20 weight:NSFontWeightBold]];
}

+ (NSMutableDictionary *)attributesWithFont:(NSFont *)font {
    return [@{ NSFontAttributeName: font,
               NSForegroundColorAttributeName: [NSColor controlTextColor] } mutableCopy];
}

+ (NSImage *)iconFromURL:(NSURL *)url {
    NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
    NSData *imageData = icon.TIFFRepresentation;
    NSString *sha256 = [FileSignature sha256StringFromData:imageData];
    if ([sha256 isEqualToString:@"4D5665BE8E0382BFFB7500F86A69117080A036B9698DC2395F3A86F9DCE7170E"] || [sha256 isEqualToString:@"48FA6B493F338A1A14423CEBCC1012C2EB9548E983A9BB6DF44A7EE5A855D2AE"]) {
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
        self.preferredContentSize = size;
    }

    [NSLayoutConstraint deactivateConstraints:@[_imageBoxHeightTracksTextBox, _imageTrailingToCenterX, _imageWidthConstraint]];

}

#pragma mark - Core Data stack

@synthesize persistentContainer = _persistentContainer;

- (NSPersistentContainer *)persistentContainer {
    @synchronized (self) {
        if (_persistentContainer == nil) {
            _persistentContainer = [ReadOnlyStore persistentContainerWithLogName:@"Spatterlight Quick Look"];
        }
    }
    return _persistentContainer;
}

- (void)preparePreviewOfSearchableItemWithIdentifier:(NSString *)identifier queryString:(NSString *)queryString completionHandler:(void (^)(NSError * _Nullable))handler {
    _hashTag = nil;

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

        if ([object isKindOfClass:[Metadata class]]) {
            metadata = (Metadata *)object;
            game = metadata.game;
        } else if ([object isKindOfClass:[Game class]]) {
            game = (Game *)object;
            metadata = game.metadata;
        } else if ([object isKindOfClass:[Image class]]) {
            image = (Image *)object;
            metadata = image.metadata.anyObject;
            game = metadata.game;
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
        handler(error);
        return;
    }

    InfoView *infoView = [[InfoView alloc] initWithFrame:self.view.bounds];

    if (game)
        url = [game urlForBookmark];

    // I guess url will always be outside the sandbox and thus inaccessible
    // so the code below will never work

    //    if (url && [Blorb isBlorbURL:url]) {
    //        Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfURL:url]];
    //        if (blorb)
    //            infoView.imageData = [blorb coverImageData];
    //    }

    // But this will
    if (url && !infoView.imageData) {
        _showingIcon = YES;
        NSImage *icon = [PreviewViewController iconFromURL:url];
        infoView.imageData = icon.TIFFRepresentation;
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

    _hashTag = nil;
    _addedFileInfo = NO;
    _showingIcon = NO;
    _showingView = NO;
    iFiction = NO;

    NSString *extension = url.pathExtension.lowercaseString;
    if ([extension isEqualToString:@"ifiction"]) {
        iFiction = YES;
        [_imageView removeFromSuperview];

        NSScrollView *scrollView = _textScrollView;
        [scrollView removeFromSuperview];
        [_largeScrollView removeFromSuperview];

        [NSLayoutConstraint deactivateConstraints:@[_textBottomToContainerBottom, _textTopToContainerTop, _textLeadingTracksImageTrailing, _textClipHeight, _imageBoxHeightTracksTextBox]];

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

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowDidEndLiveResize:)
                                                 name:NSWindowDidEndLiveResizeNotification
                                               object:nil];

    self.view.postsFrameChangedNotifications = YES;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(viewFrameChanged:)
                                                 name:NSViewFrameDidChangeNotification
                                               object:self.view];

    _textview.postsFrameChangedNotifications = YES;

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(checkForCutOffText:)
                                                 name:NSViewFrameDidChangeNotification
                                               object:_textview];

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
        if (!strongSelf)
            return;
        NSError *error = nil;
        NSString *hashTag = nil;
        Game *game = [ReadOnlyStore gameForURL:url inContext:context hashTag:&hashTag error:&error];
        if (error) {
            NSLog(@"QuickLook: fetch request failed. %@ Bailing to noPreviewForURL",error);
            [strongSelf noPreviewForURL:url handler:handler];
            giveUp = YES;
            return;
        }
        if (hashTag)
            strongSelf.hashTag = hashTag;
        if (!game) {
            metadata = [strongSelf metadataFromBlorb:url inContext:context];
            if (metadata == nil || metadata.count == 0) {
                // Found no matching path or hashTag. File not a blorb or blorb empty. Bailing to noPreviewForURL
                [strongSelf noPreviewForURL:url handler:handler];
                giveUp = YES;
                return;
            }
        }

        if (metadata == nil || metadata.count == 0) {

            Metadata *meta = game.metadata;

            NSArray<NSString*> *attributes = [NSEntityDescription
                                        entityForName:@"Metadata"
                                        inManagedObjectContext:context].attributesByName.allKeys;

            _metaDict = [[NSMutableDictionary alloc] initWithCapacity:attributes.count];

            for (NSString *attr in attributes) {
                NSObject *value = [meta valueForKey:attr];
                if (value) {
                    [_metaDict setValue:[meta valueForKey:attr] forKey:attr];
                }
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

    // A malformed file only means no embedded cover, and we fall back
    // to the database cover or file icon below.
    imageData = [Blorb coverImageDataAtURL:url];
    if (imageData)
        [_imageView addImageFromData:imageData];

    if (!_imageView.hasImage && _metaDict[@"cover"]) {
        imageData = (NSData *)_metaDict[@"cover"];
        [_imageView addImageFromData:imageData];
        _imageView.accessibilityLabel = _metaDict[@"coverArtDescription"];
    }

    [self updateWithMetadata:_metaDict url:url];
    if (_metaDict.count <= 2 && url) {
        [self addFileInfo:url];
    }

    if (!_imageView.hasImage)
        [self showIconForURL:url];

    [self makeAdjustments:handler];
    if (!_vertical)
        [self afterLayoutAdjustments];
}

#pragma mark Layout

- (BOOL)drawHorizontal {
    if (NSWidth(self.view.frame) > 50 || NSHeight(self.view.frame) > 50) {
        _largeScrollView.frame = self.view.bounds;
    }
    _largeScrollView.documentView = _horizontalView;
    _horizontalView.frame = _largeScrollView.bounds;

    NSView *superView = [InfoView addTopConstraintsToView:_horizontalView];

    [superView addConstraint:
     [NSLayoutConstraint constraintWithItem:_horizontalView
                                  attribute:NSLayoutAttributeBottom
                                  relatedBy:NSLayoutRelationEqual
                                     toItem:superView
                                  attribute:NSLayoutAttributeBottom
                                 multiplier:1.0
                                   constant:0]];

    NSSize imageSize = _imageView.originalImageSize;
    CGFloat ratio = _imageView.ratio;
    if (ratio == 0)
        ratio = 1;

    CGFloat containerWidth = NSWidth(self.view.frame);
    CGFloat containerHeight = NSHeight(self.view.frame);

    CGFloat newImageHeight, newImageWidth;
    NSRect newImageFrame, newTextFrame;

    if (!_imageRatio) {
        _imageRatio =
        [NSLayoutConstraint constraintWithItem:_imageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:_imageView
                                     attribute:NSLayoutAttributeWidth
                                    multiplier:ratio
                                      constant:0];
        _imageRatio.priority = 900;
        [_horizontalView addConstraint:_imageRatio];
    }

    if (imageSize.height >= imageSize.width) {
        // Image is narrow
        imageIsNarrow = YES;
        newImageHeight = containerHeight - 40;
        newImageWidth = ceil(newImageHeight / ratio);
    } else {
        // Image is wide
        imageIsNarrow = NO;
        newImageWidth = ceil(containerWidth * 0.5);
        newImageHeight = floor(newImageWidth * ratio);

        if (newImageHeight >= containerHeight - 40) {
            // wide image fills window vertically
            newImageHeight = containerHeight - 40;
            newImageWidth = ceil(newImageHeight / ratio);
        }
    }

    newImageFrame = NSMakeRect(20, 20, newImageWidth, newImageHeight);

    _imageWidthConstraint.constant = newImageWidth;

#pragma mark TextView

    CGFloat textHeight = [self heightForString:_textview.textStorage andWidth:containerWidth - NSMaxX(newImageFrame) - 40];

    _textClipHeight.constant = MIN(textHeight, containerHeight - 40);

    BOOL textFillsView = (_textClipHeight.constant == containerHeight - 40);

    if (_textClipHeight.constant < 20) {
        NSLog(@"Error, containerHeight is %f", containerHeight);
        _textClipHeight.constant = 245;
    }

    newTextFrame = _textScrollView.frame;
    newTextFrame.size.width = containerWidth - NSMaxX(newImageFrame) - 40;
    newTextFrame.size.height = _textClipHeight.constant;
    newTextFrame.origin.x = NSMaxX(newImageFrame) + 20;
    CGFloat newHeightOffset;
    if (textFillsView) {
        newHeightOffset = 20;
    } else {
        newHeightOffset = MAX(20, ceil((containerHeight - textHeight) / 2));
    }
    newTextFrame.origin.y = newHeightOffset;

    if (newTextFrame.size.width < 150)
        return NO;

    _textScrollView.frame = newTextFrame;

    NSRect newTextViewFrame = _textview.frame;
    newTextViewFrame.size.width = NSWidth(_textScrollView.frame);
    _textview.frame = newTextViewFrame;

    if (!imageIsNarrow && newImageHeight < _textClipHeight.constant && newImageHeight + 20 + _textTopToContainerTop.constant < containerHeight) {
        // text is taller than image
        // Pinning image at text top
        NSRect imageviewFrame = newImageFrame;
        imageviewFrame.origin.x = 0;
        imageviewFrame.origin.y = NSHeight(newTextFrame) - newImageHeight;
        newImageFrame.size.height = NSHeight(newTextFrame);
        newImageFrame.origin.y = newTextFrame.origin.y;
        _imageView.frame = imageviewFrame;
    }

    _imageBox.frame = newImageFrame;

    if (!_showingIcon)
        [self imageShadow];
    else
        _textLeadingTracksImageTrailing.constant = 0;

    [self adjustConstraints:textHeight];

    [_textScrollView.contentView scrollPoint:NSZeroPoint];

    return YES;
}

- (void)drawVertical {
    if(!_vertical) {
        NSLog(@"drawVertical: not vertical?");
        return;
    }

    [NSLayoutConstraint deactivateConstraints:@[_imageBoxHeightTracksTextBox, _imageTrailingToCenterX, _imageWidthConstraint]];

    _verticalView = [[InfoView alloc] initWithFrame:self.view.frame];
    _largeScrollView.frame = self.view.bounds;
    _largeScrollView.documentView = _verticalView;
    _verticalView.imageData = imageData;
    [_verticalView updateWithDictionary:_metaDict];
    [_largeScrollView.contentView scrollPoint:NSZeroPoint];
}

// Runs block on the main queue after delay, unless the preview is gone by then.
- (void)afterDelay:(NSTimeInterval)delay perform:(void (^)(PreviewViewController *strongSelf))block {
    PreviewViewController * __weak weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^(void){
        PreviewViewController *strongSelf = weakSelf;
        if (strongSelf)
            block(strongSelf);
    });
}

-(void)checkForChange {
    BOOL wasVertical = _vertical;
    _vertical = (NSWidth(self.view.frame) - NSHeight(self.view.frame) <= 20 || NSWidth(self.view.frame) < 300);
    if (!_vertical && _changedMyMindForWidth == NSWidth(self.view.frame))
        _vertical = YES;
    if (_vertical && !wasVertical) {
        _changing = YES;
        [self afterDelay:0.3 perform:^(PreviewViewController *strongSelf) {
            if (!strongSelf.changing)
                return;
            strongSelf.changing = NO;
            if (!strongSelf.view.inLiveResize)
                [strongSelf drawVertical];
        }];
    } else if (!_vertical && wasVertical) {
        _changing = YES;
        [self afterDelay:0.3 perform:^(PreviewViewController *strongSelf) {
            if (!strongSelf.changing)
                return;
            strongSelf.changing = NO;
            if (![strongSelf drawHorizontal]) {
                strongSelf.vertical = YES;
                strongSelf.changedMyMindForWidth = NSWidth(strongSelf.view.frame);
                if (!strongSelf.view.inLiveResize)
                    [strongSelf drawVertical];
            }
        }];
    }
}


-(void)makeAdjustments:(void (^)(NSError * _Nullable))handler  {
    _vertical = (NSWidth(self.view.frame) - NSHeight(self.view.frame) <= 20 || NSWidth(self.view.frame) < 300);
    if (_vertical) {
        // Detected as vertical
        [self drawVertical];
        _showingView = YES;
        if (handler) {
            handler(nil);
        }
    } else {
        // Detected as horizontal
        if (![self drawHorizontal] || NSWidth(_textScrollView.frame) < 150) {
            // Changed my mind
            _vertical = YES;
            _changedMyMindForWidth = NSWidth(self.view.frame);
            [self drawVertical];
            _showingView = YES;
            if (handler){
                handler(nil);
            }
            return;
        }
        _showingView = YES;
        if (handler){
            handler(nil);
        }
    }
}


-(void)afterLayoutAdjustments {
    if (iFiction)
        return;
    if (!_changing) {
        _changing = YES;
        [self afterDelay:0.3 perform:^(PreviewViewController *strongSelf) {
            strongSelf.changing = NO;
            if (!strongSelf.view.inLiveResize)
                [strongSelf makeAdjustments:nil];
        }];
    }
}

- (void)viewWillLayout {
    [super viewDidLayout];
    if (!_showingView || iFiction)
        return;

    if (_vertical || _imageView.ratio == 0 || _imageRatio == nil) {
        return;
    };

    [self adjustConstraints:[self heightForString:_textview.textStorage andWidth:NSWidth(_horizontalView.frame) - NSMaxX(_imageView.frame) - 40]];
}

- (void)viewDidLayout {
    [super viewDidLayout];
    if (!_showingView || iFiction)
        return;
    [self checkForChange];
}

- (void)checkForCutOffText:(NSNotification *)notification {
    if (notification.object == _textview && !_changing && !_vertical && !self.view.inLiveResize) {
        if (NSHeight(_textview.frame) > NSHeight(_textScrollView.frame) && NSHeight(_textScrollView.frame) < NSHeight(_horizontalView.frame) - 40 && NSHeight(_textview.frame) - NSHeight(_textScrollView.frame) < 1000) {
            _textClipHeight.constant = MIN(NSHeight(_textview.frame), NSHeight(_horizontalView.frame) - 40);
            _forcedHeightForWidth = NSWidth(_textScrollView.frame);
            self.view.needsLayout = YES;
            _changing = YES;
            [self afterDelay:0.3 perform:^(PreviewViewController *strongSelf) {
                if (!strongSelf.changing)
                    return;
                strongSelf.changing = NO;
                NSRect newFrame = strongSelf.textScrollView.frame;
                newFrame.size.height = strongSelf.textClipHeight.constant;
                newFrame.origin.y = ceil((NSHeight(strongSelf.horizontalView.frame) - newFrame.size.height) / 2);
                strongSelf.textScrollView.frame = newFrame;
                [strongSelf.textScrollView.contentView scrollPoint:NSZeroPoint];
            }];
        } else {
            [self afterDelay:0.1 perform:^(PreviewViewController *strongSelf) {
                [strongSelf.textScrollView.contentView scrollPoint:NSZeroPoint];
            }];
        }
    }
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
    if (!_showingView || iFiction)
        return;
    if ((NSWindow *)notification.object == self.view.window) {
        // window matched
        _changing = NO;
        [self afterDelay:0.3 perform:^(PreviewViewController *strongSelf) {
            strongSelf.changing = NO;
            if (!strongSelf.view.inLiveResize)
                [strongSelf makeAdjustments:nil];
        }];
    }
}

- (void)viewFrameChanged:(NSNotification *)notification {
    if (!_showingView || iFiction)
        return;
    if ((NSView *)notification.object == self.view) {
        // window matched
        [self makeAdjustments:nil];
        [self checkForCutOffText:[NSNotification notificationWithName:NSViewFrameDidChangeNotification object:_textview]];
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

    NSTextView *textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, textWidth, 0)];
    textView.verticallyResizable = YES;
    [textView.textStorage appendAttributedString:attString];
    [textView.layoutManager ensureLayoutForTextContainer:textView.textContainer];
    CGRect proposedRect =
    textView.frame;
    return ceil(proposedRect.size.height);
}

- (void)adjustConstraints:(CGFloat)textHeight {

    if (_forcedHeightForWidth != NSWidth(_textScrollView.frame)) {
        _textClipHeight.constant = MIN(textHeight, NSHeight(_horizontalView.frame) - 40);
    }

    if (_textClipHeight.constant < 20) {
        _textClipHeight.constant = 40;
    }

    CGFloat newImageHeight = NSHeight(_horizontalView.frame) - 40;
    CGFloat newImageWidth = ceil(newImageHeight / _imageView.ratio);

    [NSLayoutConstraint activateConstraints:@[_imagePinnedToContainerBottom, _imageTopToContainerTop, _imageBottomToContainerBottom, _centerImageVertically, _imageRatio]];

    [NSLayoutConstraint deactivateConstraints:@[_imageBoxHeightTracksTextBox, _imageTrailingToCenterX, _imageWidthConstraint]];

    if (!imageIsNarrow) {
        // Image is wide
        _imageTrailingToCenterX.active = YES;
        newImageWidth = ceil(NSWidth(_horizontalView.frame) * 0.5);
        newImageHeight = floor(newImageWidth * _imageView.ratio);

        if (newImageHeight >= NSHeight(_horizontalView.frame) - 40) {
            // wide image fills vertically;
            _imagePinnedToContainerBottom.active = YES;
            _imageTrailingToCenterX.active = NO;
        } else if (newImageHeight < _textClipHeight.constant) {
            // text is taller than image
            // Pinning image at text top
            _imagePinnedToContainerBottom.active = NO;
            _imageBoxHeightTracksTextBox.active = YES;
        }
    } else {
        // Image is narrow
        _imageWidthConstraint.constant = newImageWidth;
        _imageWidthConstraint.active = YES;
    }
}



#pragma mark Adding metadata

// With no cover image, show the file's icon instead. The house means the
// Finder has no icon of its own for it.
- (void)showIconForURL:(NSURL *)url {
    _showingIcon = YES;
    NSImage *image = [PreviewViewController iconFromURL:url];
    if (image == [NSImage imageNamed:@"house"])
        _showingIcon = NO;
    NSData *iconData = image.TIFFRepresentation;
    _metaDict[@"cover"] = iconData;
    // If we add the icon image directly it becomes low resolution and blurry
    [_imageView addImageFromData:iconData];
}

- (void)noPreviewForURL:(NSURL *)url handler:(void (^)(NSError *))handler {
    NSMutableDictionary *attrDict = [PreviewViewController titleAttributes];
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

    NSString *ifid = @"";

    if (_metaDict[@"ifid"])
        ifid = (NSString *)_metaDict[@"ifid"];
    if (!ifid.length) {
        ifid = [PreviewViewController ifidFromFile:url.path];
    }
    if (ifid.length) {
        [self addInfoLine:[@"IFID: " stringByAppendingString:ifid] attributes:attrDict linebreak:YES];
        _metaDict[@"ifid"] = ifid;
    }

    [self addFileInfo:url];

    if (!_imageView.hasImage)
        [self showIconForURL:url];

    [self makeAdjustments:handler];
    if (!_vertical)
        [self afterLayoutAdjustments];
}

- (void)updateWithMetadata:(NSDictionary *)metadict url:(nullable NSURL *)url {
    if (metadict && metadict.count) {
        NSMutableDictionary *attrDict = [PreviewViewController titleAttributes];
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


        NSString *headlineString = (NSString *)metadict[@"headline"];
        NSString *authorString = (NSString *)metadict[@"author"];
        NSString *blurbString = (NSString *)metadict[@"blurb"];

        BOOL noMeta = (headlineString.length + authorString.length + blurbString.length == 0);

        [self addStarRating:metadict];
        attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        [self addInfoLine:headlineString attributes:attrDict linebreak:YES];
        [self addInfoLine:authorString attributes:attrDict linebreak:YES];
        if (!authorString.length)
            [self addInfoLine:metadict[@"AUTH"] attributes:attrDict linebreak:YES];
        [self addInfoLine:blurbString attributes:attrDict linebreak:YES];

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
    NSMutableDictionary *attrDict = [PreviewViewController attributesWithFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];

    NSError *error = nil;
    NSDictionary * fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:url.path error:&error];
    if (fileAttributes) {
        NSDate *modificationDate = (NSDate *)fileAttributes[NSFileModificationDate];
        [self addInfoLine:[NSString stringWithFormat:@"Last modified: %@", [InfoView formattedStringFromDate:modificationDate]] attributes:attrDict linebreak:YES];
        NSNumber *fileSizeObj = (NSNumber *)fileAttributes[NSFileSize];
        NSInteger fileSize = 0;
        if (fileSizeObj)
            fileSize = fileSizeObj.integerValue;
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
    if (mdItem == nil)
        return nil;
    CFTypeRef dateRef = MDItemCopyAttribute(mdItem, kMDItemLastUsedDate);
    NSDate *date = CFBridgingRelease(dateRef);
    CFRelease(mdItem);
    CFRelease(cfurl);
    return date;
}

// Must be called from inside a perform block on context, which is used
// for the IFhd title lookup.
- (NSMutableDictionary *)metadataFromBlorb:(NSURL *)url inContext:(NSManagedObjectContext *)context {
    if (![Blorb isBlorbURL:url])
        return nil;

    NSMutableDictionary *metaDict = [NSMutableDictionary new];

    NSData *data = nil;

    // This is called from inside a Core Data perform block, so an exception
    // from a malformed file would be rethrown and crash the extension.
    // Treat it as a file without Blorb metadata instead.
    Blorb *blorb = [Blorb blorbAtURL:url];
    @try {
        metaDict[@"cover"] = [blorb coverImageData];

        metaDict[@"IFhd"] = [blorb ifidFromIFhd];
        if (metaDict[@"IFhd"]) {
            metaDict[@"IFhdTitle"] = [self titleFromIfid:metaDict[@"IFhd"] inContext:context];
        } else NSLog(@"No IFdh resource in Blorb file");

        for (NSString *chunkName in blorb.optionalChunks.allKeys) {
            metaDict[chunkName] = blorb.optionalChunks[chunkName];
            //        NSLog(@"metaDict[%@] = \"%@\"", chunkName, metaDict[chunkName]);
        }

        data = blorb.metaData;
    } @catch (NSException *exception) {
        NSLog(@"QuickLook: Exception parsing Blorb file %@: %@", url.path, exception);
        return nil;
    }

    // A Blorb holds the iFiction record of one story
    IFStory *story = data ? [[IFictionMetadata alloc] initWithData:data].firstStory : nil;
    IFBibliographic *bibliographic = story.bibliographic;
    if (bibliographic.title.length)
        metaDict[@"title"] = bibliographic.title;
    if (bibliographic.author.length)
        metaDict[@"author"] = bibliographic.author;
    if (bibliographic.headline.length)
        metaDict[@"headline"] = bibliographic.headline;
    if (bibliographic.blurb.length)
        metaDict[@"blurb"] = bibliographic.blurb;

    // Prefer the IFID the Blorb identifies itself with, if the record lists it
    NSSet<NSString *> *ifids = story.identification.ifids;
    NSString *ifhd = metaDict[@"IFhd"];
    if (ifhd.length && [ifids containsObject:ifhd])
        metaDict[@"ifid"] = ifhd;
    else if (ifids.count)
        metaDict[@"ifid"] = [ifids.allObjects sortedArrayUsingSelector:@selector(compare:)].firstObject;
    return metaDict;
}

// Must be called from inside a perform block on context, so that callers
// already running on a context's queue can reuse it instead of creating a
// second context and nesting another performBlockAndWait inside the first.
- (NSString *)titleFromIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    if (!ifid || !context)
        return nil;

    Game *game = [ReadOnlyStore gameMatching:[NSPredicate predicateWithFormat:@"ifid ==[c] %@", ifid]
                                   inContext:context
                                       error:NULL];
    if (!game)
        return nil;
    [self addImageFromGame:game];
    return game.metadata.title;
}

- (NSString *)titleFromIfid:(NSString *)ifid {
    if (!ifid)
        return nil;
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"Could not create new context!");
        return nil;
    }
    NSString __block *title = nil;

    // Managed objects may only be accessed on their context's queue,
    // so read everything we need from the game inside the block
    [context performBlockAndWait:^{
        title = [self titleFromIfid:ifid inContext:context];
    }];

    return title;
}

- (NSString *)titleFromHash:(NSString *)hash {
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"context is nil!");
        return nil;
    }
    NSString __block *title = nil;

    // Managed objects may only be accessed on their context's queue,
    // so read everything we need from the game inside the block
    [context performBlockAndWait:^{
        Game *game = [ReadOnlyStore gameMatching:[NSPredicate predicateWithFormat:@"serialString ==[c] %@", hash]
                                       inContext:context
                                           error:NULL];
        if (game) {
            [self addImageFromGame:game];
            title = game.metadata.title;
        }
    }];

    return title;
}

+ (NSString *)ifidFromFile:(NSString *)path {
    if (!path.length)
        return nil;
    void *context = get_babel_ctx();
    if (context == NULL) {
        return nil;
    }

    int rv = 0;
    char buf[TREATY_MINIMUM_EXTENT];

    char *format = babel_init_ctx((char *)path.fileSystemRepresentation, context);
    if (format && babel_get_authoritative_ctx(context)) {
        rv = babel_treaty_ctx(GET_STORY_FILE_IFID_SEL, buf, sizeof buf, context);
    }

    babel_release_ctx(context);
    free(context);

    if (rv <= 0) {
        return nil;
    }

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
    NSString __block *title = nil;
    NSString *ifid = nil;
    if (data) {
        const unsigned char *bytes = data.bytes;
        NSUInteger dataLength = data.length;
        uint32_t chunkID, length;
        IFhdInfo info;
        if ((isForm(bytes, dataLength, 'I', 'F', 'Z', 'S') || isForm(bytes, dataLength, 'B', 'F', 'Z', 'S')) &&
            chunkHeaderAt(bytes, dataLength, 12, &chunkID, &length) && chunkID == IFFID('I', 'F', 'h', 'd') &&
            length <= dataLength - 20 && parseIFhd(bytes + 20, length, &info)) {
            // Game Identifier Chunk
            NSUInteger releaseNumber = info.release;
            NSUInteger checksum = info.checksum;
            NSString *serialNum = [[NSString alloc] initWithBytes:info.serial length:6 encoding:NSASCIIStringEncoding];

            // See if we can find a path to the game file as well
            NSString __block *path = nil;
            NSUInteger next = 20 + paddedLength(length);
            if (chunkHeaderAt(bytes, dataLength, next, &chunkID, &length) &&
                chunkID == IFFID('I', 'n', 't', 'D') &&
                length >= 12 && length <= dataLength - next - 8) {
                // Found IntD chunk! The path follows its 12-byte header.
                NSData *pathData = [data subdataWithRange:NSMakeRange(next + 20, length - 12)];
                path = [[NSString alloc] initWithData:pathData encoding:NSASCIIStringEncoding];
            }

            NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
            if (context) {
                // Managed objects may only be accessed on their context's
                // queue, so read everything we need from the game in here
                [context performBlockAndWait:^{
                    Game *game = [ReadOnlyStore gameMatching:[NSPredicate predicateWithFormat:@"serialString ==[c] %@ AND releaseNumber == %d AND checksum == %d", serialNum, releaseNumber, checksum]
                                                   inContext:context
                                                       error:NULL];

                    //If no match, look for path
                    if (!game && path.length) {
                        game = [ReadOnlyStore gameMatching:[NSPredicate predicateWithFormat:@"path ==[c] %@", path]
                                                 inContext:context
                                                     error:NULL];
                    }

                    if (game) {
                        title = game.metadata.title;
                        [self addImageFromGame:game];
                    }
                }];
            }

            if (!title) {
                if (path.length) {
                    title = path.lastPathComponent;
                } else if (!ifid) {
                    ifid = [Blorb zcodeIFIDWithRelease:releaseNumber serial:serialNum ?: @"" checksum:checksum];
                    title = [self titleFromIfid:ifid];
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
    // The first 64 bytes of the IFhd payload, which for Glulx saves is the
    // start of the game file header.
    uint32_t chunkID, length;
    if (data.length >= 84 && isForm(data.bytes, data.length, 'I', 'F', 'Z', 'S') &&
        chunkHeaderAt(data.bytes, data.length, 12, &chunkID, &length) &&
        chunkID == IFFID('I', 'F', 'h', 'd')) {
        return [FileSignature hexStringFromData:[data subdataWithRange:NSMakeRange(20, 64)]].lowercaseString;
    }
    return nil;
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
            _metaDict[@"coverArtDescription"] = game.metadata.cover.imageDescription;
        }
    }
}

@end

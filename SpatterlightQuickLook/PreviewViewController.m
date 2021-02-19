//
//  PreviewViewController.m
//  SpatterlightQuickLook
//
//  Created by Administrator on 2021-01-29.
//
#import <Cocoa/Cocoa.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

#import <Quartz/Quartz.h>
#import <CoreData/CoreData.h>

#import "Game.h"
#import "Metadata.h"
#import "Image.h"

#import "PreviewViewController.h"
#import "Blorb.h"

/* the treaty of babel headers */
#include "babel_handler.h"

@interface ConstraintLessView : NSView

- (void)addSizableMasks;

- (void)removeAllConstraints;

@end

@implementation ConstraintLessView

- (void)removeAllConstraints
{
    NSView *superview = self.superview;
    while (superview != nil) {
        for (NSLayoutConstraint *c in superview.constraints) {
            if (c.firstItem == self || c.secondItem == self) {
                [superview removeConstraint:c];
            }
        }
        superview = superview.superview;
    }

    [self removeConstraints:self.constraints];
    self.translatesAutoresizingMaskIntoConstraints = YES;
}

- (void)addSizableMasks {
    self.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
}

@end



@interface PreviewViewController () <QLPreviewingController, NSTextViewDelegate>

@end

@implementation PreviewViewController

- (NSString *)nibName {
    return @"PreviewViewController";
}

- (void)loadView {
    [super loadView];
    NSLog(@"loadView");
    NSLog(@"self.view.frame %@", NSStringFromRect(self.view.frame));

    self.view.translatesAutoresizingMaskIntoConstraints = NO;
    _preferredWidth = 582;
    // Do any additional setup after loading the view.
}

#pragma mark - Core Data stack

@synthesize persistentContainer = _persistentContainer;

- (NSPersistentContainer *)persistentContainer {
    @synchronized (self) {
        if (_persistentContainer == nil) {
            _persistentContainer = [[NSPersistentContainer alloc] initWithName:@"Spatterlight"];

            NSString *groupIdentifier =
                [[NSBundle mainBundle] objectForInfoDictionaryKey:@"GroupIdentifier"];

            NSURL *directory = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];

            NSURL *url = [NSURL fileURLWithPath:[directory.path stringByAppendingPathComponent:@"Spatterlight.storedata"]];

            NSPersistentStoreDescription *description = [[NSPersistentStoreDescription alloc] initWithURL:url];

            description.readOnly = YES;
            description.shouldMigrateStoreAutomatically = NO;

            _persistentContainer.persistentStoreDescriptions = @[ description ];

            NSLog(@"persistentContainer url path:%@", url.path);

            [_persistentContainer loadPersistentStoresWithCompletionHandler:^(NSPersistentStoreDescription *description, NSError *error) {
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
//- (void)preparePreviewOfSearchableItemWithIdentifier:(NSString *)identifier queryString:(NSString *)queryString completionHandler:(void (^)(NSError * _Nullable))handler {
//    NSLog(@"preparePreviewOfSearchableItemWithIdentifier");
//    NSLog(@"Identifier: %@", identifier );
//    NSLog(@"queryString: %@", queryString );
//
//
//    // Perform any setup necessary in order to prepare the view.
//    
//    // Call the completion handler so Quick Look knows that the preview is fully loaded.
//    // Quick Look will display a loading spinner while the completion handler is not called.
//
//    handler(nil);
//}

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {
    NSLog(@"preparePreviewOfFileAtURL");
    NSLog(@"self.view.frame %@", NSStringFromRect(self.view.frame));

    _ifid = nil;
    _addedFileInfo = NO;
    _showingIcon = NO;
    // Add the supported content types to the QLSupportedContentTypes array in the Info.plist of the extension.
    
    // Perform any setup necessary in order to prepare the view.
    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.

    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"context is nil!");
        [self noPreviewForURL:url handler:handler];
        return;
    }

    __block NSMutableDictionary *metadata = nil;

    __unsafe_unretained PreviewViewController *weakSelf = self;
    [context performBlockAndWait:^{
        NSError *error = nil;
        NSArray *fetchedObjects;

        NSFetchRequest *fetchRequest = [NSFetchRequest new];

        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path like[c] %@", url.path];

        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects == nil) {
            NSLog(@"QuickLook: %@",error);
            [weakSelf noPreviewForURL:url handler:handler];
            return;
        }
        if (fetchedObjects.count == 0) {
//            NSLog(@"QuickLook: Found no Game object with path %@", url.path);
            weakSelf.ifid = [weakSelf ifidFromFile:url.path];
            if (weakSelf.ifid) {
                fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@", weakSelf.ifid];
                fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
            }
            if (fetchedObjects.count == 0) {
//                NSLog(@"QuickLook: Found no Game object with ifid  %@", weakSelf.ifid);
                metadata = [weakSelf metadataFromURL:url];
                if (metadata == nil || metadata.count == 0) {
                    [weakSelf noPreviewForURL:url handler:handler];
                    return;
                } // else NSLog(@"Found metadata in blorb");
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
                //NSLog(@"Setting my %@ to %@", attr, [theme valueForKey:attr]);
                [metadata setValue:[meta valueForKey:attr] forKey:attr];
            }
            metadata[@"ifid"] = game.ifid;
            metadata[@"cover"] = game.metadata.cover.data;
            metadata[@"lastPlayed"] = game.lastPlayed;
        }

    if (metadata == nil || metadata.count == 0) {
        [weakSelf noPreviewForURL:url handler:handler];
        return;
    }
    }];

    dispatch_async(dispatch_get_main_queue(), ^{
        if (metadata[@"cover"]) {
            weakSelf.imageView.image = [[NSImage alloc] initWithData:(NSData *)metadata[@"cover"]];
            weakSelf.imageView.accessibilityLabel = metadata[@"coverArtDescription"];
        } else {
            weakSelf.showingIcon = YES;
            weakSelf.imageView.image = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
        }

        NSSize viewSize = self.view.frame.size;
        self.vertical = (viewSize.width - viewSize.height <= 20);
        if (!self.vertical) {
            [self tryToStretchWindow];
        }

        [weakSelf sizeImage];
        weakSelf.textview.hidden = YES;
        [weakSelf updateWithMetadata:metadata url:url];
        if (metadata.count <= 2) {
            [self addFileInfo:url];
        }
        [self sizeText];
        double delayInSeconds = 0.1;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            [self sizeText];
            self.textview.hidden = NO;
            [self printFinalLayout];
        });
        //            delayInSeconds = 0.2;
        //            popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        //            dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        //                [weakSelf sizeText];
        //            });

    });
    handler(nil);

}

- (void)printFinalLayout{
    NSLog(@"Final layout: view size: %@ image view frame %@ scroll view frame:%@", NSStringFromSize(self.view.frame.size), NSStringFromRect(_imageView.frame), NSStringFromRect(_textview.enclosingScrollView.frame));
    NSLog(@"Layout is %@", _vertical ? @"vertical" : @"horizontal");
}

- (void)sizeImageToFitWidth:(CGFloat)maxWidth height:(CGFloat)maxHeight {

    NSSize imgsize;

    imgsize = _originalImageSize;
    if (imgsize.width == 0 || imgsize.height == 0) {
        imgsize = _imageView.image.size;
        _originalImageSize = imgsize;
    }

    if (imgsize.height == 0)
        return;

    CGFloat ratio = imgsize.width / imgsize.height;

    imgsize.height = maxHeight;
    imgsize.width = imgsize.height * ratio;

    if (imgsize.width > maxWidth) {
        imgsize.width = maxWidth;
        imgsize.height =  imgsize.width / ratio;
    }

//    if (_originalImageSize.width < 128 && _originalImageSize.width > 0)
//    {
//        _imageView.wantsLayer = YES;
//        _imageView.layer.magnificationFilter = kCAFilterNearest; //Nearest neighbor texture filtering
//
//        [_imageView setFrameSize:imgsize];
//        [self nearestNeighbor: imgsize.width / _originalImageSize.width];
//
////        _imageView.image.size = imgsize;
//    } else {
        _imageView.image.size = imgsize;
//    }

    //    imgsize.height = self.view.frame.size.height;
    [_imageView setFrameSize:imgsize];
}

- (void)nearestNeighbor:(CGFloat)magnification {
    NSImage *image = _imageView.image;
    _imageView.image = nil;

    image.size = NSMakeSize(16, 16);

    CALayer *layer = [CALayer layer];
    layer.magnificationFilter = kCAFilterNearest;
    layer.frame = CGRectMake(0, 0, image.size.width, image.size.height);
    layer.contents = (__bridge id _Nullable)[image CGImageForProposedRect:NULL context:nil hints:nil];

    [_imageView setLayer:[CALayer layer]];
    [_imageView setWantsLayer:YES];
    [_imageView.layer addSublayer:layer];

    CGFloat scale = floor(magnification * _originalImageSize.width / 16);
    if (scale / 2 != floor(scale / 2))
        scale = scale + 1;
    NSLog(@"PIXEL SCALE = %f", scale);
    _imageView.layer.magnificationFilter = kCAFilterNearest; //Nearest neighbor texture filtering
    _imageView.layer.sublayerTransform = CATransform3DMakeScale(scale, scale, 1); //Scale layer up
    //Rasterize w/ sufficient resolution to show sharp pixels
    _imageView.layer.shouldRasterize = YES;
    _imageView.layer.rasterizationScale = scale;
}


- (void)sizeImage {
    if (!_imageView.image)
        return;
    _imageView.imageScaling = NSImageScaleProportionallyUpOrDown;
    if (!_vertical) {
        [self sizeImageHorizontally];
    } else [self sizeImageVertically];

    [self imageShadow];
}

- (void)imageShadow {
    NSShadow *shadow = [NSShadow new];
    shadow.shadowOffset = NSMakeSize(1, -1);
    shadow.shadowColor = [NSColor controlShadowColor];
    shadow.shadowBlurRadius = 1;
    _imageView.wantsLayer = YES;
    _imageView.superview.wantsLayer = YES;
    _imageView.shadow = shadow;
}

- (void)sizeImageHorizontally {
    _scrollviewImageviewVerticalConstraint.active = NO;
    _imageviewTrailingConstraint.active = NO;
    NSSize viewSize = self.view.frame.size;

    [self sizeImageToFitWidth:round(2 * viewSize.width / 3 - 40) height:256];
    _imageView.imageAlignment =  NSImageAlignLeft;
    NSRect frame = _imageView.frame;
    frame.size.height = viewSize.height;
    frame.size.width = _imageView.image.size.width;
    frame.origin.y = 0;
    frame.origin.x = 20;
    _imageView.frame = frame;
    // 256 is the default height of Finder file previews minus margins
}

- (void)sizeImageVertically {
    NSLog(@"sizeImageVertically");

    _scrollviewImageviewHorizontalConstraint.active = NO;

    _scrollviewImageviewVerticalConstraint.active = YES;
    _scrollviewImageviewVerticalConstraint.priority = 1000;
    _scrollviewImageviewVerticalConstraint.constant = 20;

    _imageviewTrailingConstraint.active = YES;
    _imageviewTrailingConstraint.constant = 20;
    _imageviewTrailingConstraint.priority = 750;

    _imageviewBottomConstraint.active = NO;

    _scrollviewLeadingConstraint.active = YES;
    _scrollviewLeadingConstraint.priority = 1000;
    _scrollviewLeadingConstraint.constant = 20;
    _scrollviewTopConstraint.active = NO;
    _scrollviewBottomConstraint.constant = 10;


    NSSize viewSize = _imageView.superview.frame.size;

    //    [_imageView removeFromSuperview];
    //We want the image to be at  amost two thirds of the view height
    [self sizeImageToFitWidth:viewSize.width - 40 height:round(viewSize.height / 2)];
    // 256 is the default height of Finder file previews minus margins

    NSRect frame = _imageView.frame;
    frame.size.height = _imageView.image.size.height + 20;
    frame.size.width = viewSize.width - 40;
    frame.origin.y = viewSize.height - frame.size.height - 20;
    frame.origin.x = 20;
    _imageView.frame = frame;
    _imageView.imageAlignment =  NSImageAlignTopLeft;
    NSLog(@"sizeImageVertically: image view frame : %@", NSStringFromRect(_imageView.frame));

    //    [self.view addSubview:_imageView];
}

- (void)sizeText {
    if (!_vertical) {
        [self sizeTextHorizontally];
    } else [self sizeTextVertically];
}

- (void)tryToStretchWindow {
    CGFloat preferredWindowWidth = 612;
    CGFloat preferredWindowHeight = 296;
    CGFloat preferredImageWidth = 408;
    CGFloat preferredImageHeight = 256;

    NSSize imgsize;

    imgsize = _originalImageSize;
    if (imgsize.width == 0 || imgsize.height == 0) {
        imgsize = _imageView.image.size;
        _originalImageSize = imgsize;
    }

    if (imgsize.height == 0)
        return;

    CGFloat ratio = imgsize.width / imgsize.height;
    if (preferredImageHeight * ratio > preferredImageWidth && !self.showingIcon) {
        preferredWindowWidth = round(preferredWindowHeight * ratio * 1.5);
        NSLog(@"Image too wide! 256 * ratio (%f) = %f > 408. Trying to stretch window to width %f", ratio, 256 * ratio, preferredWindowWidth);
    }

    if (preferredWindowWidth > self.view.window.screen.visibleFrame.size.width / 3)
        preferredWindowWidth = self.view.window.screen.visibleFrame.size.width / 3;

    self.preferredContentSize = NSMakeSize(preferredWindowWidth, preferredWindowHeight);
}

- (void)sizeTextHorizontally {
    _scrollviewLeadingConstraint.active = NO;
    NSScrollView *scrollView = _textview.enclosingScrollView;
    NSRect frame = scrollView.frame;

    // The icon image usually has horizontal padding built-in
    frame.origin.x = NSMaxX(_imageView.frame) + (_showingIcon ? 10 : 20);
    frame.size.width = self.view.frame.size.width - frame.origin.x - 20;
    scrollView.frame = frame;

    [_textview.layoutManager glyphRangeForTextContainer:_textview.textContainer];
    CGFloat textHeight = [_textview.layoutManager
                          usedRectForTextContainer:_textview.textContainer].size.height;
    if (textHeight < self.view.frame.size.height - 50) {
        frame.size.height = MIN(textHeight + _textview.textContainerInset.height * 2, self.view.frame.size.height - 20);
        frame.origin.y = ceil((self.view.frame.size.height - textHeight) / 2);
        if (frame.origin.y < 0)
            frame.origin.y = 0;
    }

    scrollView.frame = frame;
    _scrollviewBottomConstraint.constant = frame.origin.y;
    _scrollviewTopConstraint.constant = frame.origin.y;
    frame = _textview.frame;
    frame.size.width = scrollView.frame.size.width;
    _textview.frame = frame;

    [scrollView.contentView scrollToPoint:NSZeroPoint];
}

- (void)sizeTextVertically {
    NSLog(@"sizeTextVertically");

    _scrollviewImageviewHorizontalConstraint.active = NO;

    _scrollviewImageviewVerticalConstraint.active = YES;
    _scrollviewImageviewVerticalConstraint.priority = 1000;
    _scrollviewImageviewVerticalConstraint.constant = 20;

    _imageviewTrailingConstraint.active = YES;
    _imageviewTrailingConstraint.constant = 20;
    _imageviewTrailingConstraint.priority = 750;

    _imageviewBottomConstraint.active = NO;

    _scrollviewLeadingConstraint.active = YES;
    _scrollviewLeadingConstraint.priority = 1000;
    _scrollviewLeadingConstraint.constant = 20;
    _scrollviewTopConstraint.active = NO;
    _scrollviewBottomConstraint.constant = 10;

    NSScrollView *scrollView = _textview.enclosingScrollView;
    NSRect frame = scrollView.frame;

    NSSize viewSize = scrollView.superview.frame.size;

    frame.size.height = viewSize.height - _imageView.frame.size.height - 20;

    frame.size.width = viewSize.width - 40;

    if (frame.size.height < viewSize.height / 2 - 20)
    {
        NSLog(@"That is too small! Trying to resize image height to %f", round (viewSize.height / 2));
        [self sizeImageToFitWidth:self.view.frame.size.width height:round (viewSize.height / 2)];
        frame.size.height = viewSize.height - _imageView.frame.size.height - 20;
        NSLog(@"New height %f", frame.size.height);
    }

    frame.origin = NSMakePoint(20, 0);

    scrollView.frame = frame;

    frame = _textview.frame;
    frame.size.width = scrollView.frame.size.width;
    _textview.frame = frame;

    [scrollView.contentView scrollToPoint:NSZeroPoint];
}

- (void)updateWithMetadata:(NSDictionary *)metadict url:(NSURL *)url {

    if (metadict && metadict.count) {
        NSFont *systemFont = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
        NSMutableDictionary *attrDict = [NSMutableDictionary new];
        attrDict[NSFontAttributeName] = systemFont;
        attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
        [self addInfoLine:metadict[@"title"] attributes:attrDict linebreak:NO];

        if (!metadict[@"title"]) {
            [self addInfoLine:url.path.lastPathComponent attributes:attrDict linebreak:YES];

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
        [self addInfoLine:metadict[@"AUTH"] attributes:attrDict linebreak:YES];
        [self addInfoLine:metadict[@"blurb"] attributes:attrDict linebreak:YES];

        [self addInfoLine:metadict[@"ANNO"] attributes:attrDict linebreak:YES];
        if (metadict[@"(c) "])
            [self addInfoLine:[NSString stringWithFormat:@"© %@", metadict[@"(c) "]] attributes:attrDict linebreak:YES];

        NSDate * lastPlayed = metadict[@"lastPlayed"];
        NSDateFormatter *formatter = [NSDateFormatter new];
        formatter.dateFormat = @"dd MMM yyyy HH.mm";
        // It looks better with the last played line last if we show file info,
        // so that modification date follows last played date and the two dates are grouped
        if (lastPlayed && !noMeta) {
            [self addInfoLine:[NSString stringWithFormat:@"Last played %@", [formatter stringFromDate:lastPlayed]] attributes:attrDict linebreak:YES];
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
            [self addInfoLine:[NSString stringWithFormat:@"Last played %@", [formatter stringFromDate:lastPlayed]] attributes:attrDict linebreak:YES];
        if (noMeta)
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
    //+ (NSAttributedString *)starWithRating:(CGFloat)rating
    //                            outOfTotal:(NSInteger)totalNumberOfStars
    //                          withFontSize:(CGFloat) fontSize {
    NSInteger rating = NSNotFound;
    if (dict[@"myRating"]) {
        rating = ((NSNumber *)dict[@"myRating"]).integerValue;
    } else if  (dict[@"starRating"]) {
        rating = ((NSNumber *)dict[@"starRating"]).integerValue;
    }

    if (rating == NSNotFound)
        return;

    NSUInteger totalNumberOfStars = 5;
    NSFont *currentFont = [NSFont fontWithName:@"SF Pro" size:12];
    if (!currentFont)
        currentFont = [NSFont systemFontOfSize:20 weight:NSFontWeightRegular];

    if (@available(macOS 10.13, *)) {
        NSDictionary *activeStarFormat = @{
            NSFontAttributeName : currentFont,
            NSForegroundColorAttributeName : [NSColor colorNamed:@"customControlColor"]
        };
        NSDictionary *inactiveStarFormat = @{
            NSFontAttributeName : currentFont,
            NSForegroundColorAttributeName : [NSColor colorNamed:@"customControlColor"]
        };

        NSMutableAttributedString *starString = [NSMutableAttributedString new];
        [starString appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:@"\n\n" attributes:activeStarFormat]];

        for (int i=0; i < totalNumberOfStars; ++i) {
            //Full star
            if (rating >= i+1) {
                [starString appendAttributedString:[[NSAttributedString alloc]
                                                    initWithString:@"􀋃 " attributes:activeStarFormat]];
            }
            //Half star
            else if (rating > i) {
                [starString appendAttributedString:[[NSAttributedString alloc]
                                                    initWithString:@"􀋄 " attributes:activeStarFormat]];
            }
            // Grey star
            else {
                [starString appendAttributedString:[[NSAttributedString alloc]
                                                    initWithString:@"􀋂 " attributes:inactiveStarFormat]];
            }
        }
        [_textview.textStorage appendAttributedString:starString];
    }
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

- (void)noPreviewForURL:(NSURL *)url handler:(void (^)(NSError *))handler {
    NSLog(@"noPreviewForURL");
    _showingIcon = YES;
    _imageView.image = [[NSWorkspace sharedWorkspace] iconForFile:url.path];
    NSFont *systemFont = [NSFont systemFontOfSize:20 weight:NSFontWeightBold];
    NSMutableDictionary *attrDict = [NSMutableDictionary new];
    attrDict[NSFontAttributeName] = systemFont;
    attrDict[NSForegroundColorAttributeName] = [NSColor controlTextColor];
    [self addInfoLine:url.path.lastPathComponent attributes:attrDict linebreak:NO];

    if ([url.path.pathExtension.lowercaseString isEqualToString:@"d$$"]){
        attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        [self addInfoLine:@"Possibly an AGT game. Try opening in Spatterlight to convert to AGX format." attributes:attrDict linebreak:YES];
    }
    if (_ifid && _ifid.length) {
        attrDict[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        [self addInfoLine:[@"IFID: " stringByAppendingString:_ifid] attributes:attrDict linebreak:YES];
    }

    [self addFileInfo:url];

    NSSize viewSize = self.view.frame.size;

    if (viewSize.width - viewSize.height > 20 ) {
        [self tryToStretchWindow];
    }
    [self sizeImage];
    [self sizeText];
    handler(nil);
    double delayInSeconds = 0.1;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [self sizeText];
        self.textview.hidden = NO;
        [self printFinalLayout];
    });
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
        NSDateFormatter *formatter = [NSDateFormatter new];
        formatter.dateFormat = @"d MMM yyyy HH.mm";
        [self addInfoLine:[NSString stringWithFormat:@"Last modified %@", [formatter stringFromDate:modificationDate]] attributes:attrDict linebreak:YES];
        NSInteger fileSize = ((NSNumber *)fileAttributes[NSFileSize]).integerValue;
        [self addInfoLine:[self unitStringFromBytes:fileSize] attributes:attrDict linebreak:YES];
    } else {
        NSLog(@"Could not read file attributes!");
    }
}


- (NSMutableDictionary *)metadataFromURL:(NSURL *)url {
    NSMutableDictionary *metaDict = [NSMutableDictionary new];
    if (![Blorb isBlorbURL:url])
        return nil;

    Blorb *blorb = [[Blorb alloc] initWithData:[NSData dataWithContentsOfFile:url.path]];
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
                NSXMLElement *idElement = [story elementsForName:@"identification"].firstObject;
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
    NSManagedObjectContext *context = self.persistentContainer.newBackgroundContext;
    if (!context) {
        NSLog(@"context is nil!");
        return @"";;
    }
    __block Game *game;

    [context performBlockAndWait:^{
        NSError *error = nil;
        NSArray *fetchedObjects;

        NSFetchRequest *fetchRequest = [NSFetchRequest new];

        fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:context];
        fetchRequest.predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@", ifid];

        fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        if (fetchedObjects && fetchedObjects.count) {
            game = fetchedObjects[0];
        }
    }];

    return game.metadata.title;
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

@end


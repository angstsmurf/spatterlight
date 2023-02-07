#import "InfoController.h"

#import "Game.h"
#import "Metadata.h"
#import "AppDelegate.h"
#import "TableViewController.h"
#import "CoreDataManager.h"
#import "Image.h"
#import "IFDBDownloader.h"
#import "ImageView.h"

#import "Constants.h"

#import <QuartzCore/QuartzCore.h>

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
fprintf(stderr, "%s\n",                                                    \
[[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface InfoPanel : NSWindow

@property BOOL disableConstrainedWindow;

@end

@implementation InfoPanel

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen {
    return (_disableConstrainedWindow ? frameRect : [super constrainFrameRect:frameRect toScreen:screen]);
}

@end

@interface HelperView : NSView

@end

@implementation HelperView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    InfoController *infocontroller = (InfoController *)self.window.delegate;

    unichar code = [event.characters characterAtIndex:0];

    switch (code) {
        case NSUpArrowFunctionKey: {
            if (infocontroller.inAnimation) {
                infocontroller.upArrowWhileInAnimation = YES;
                infocontroller.downArrowWhileInAnimation = NO;
            } else {
                [infocontroller.libcontroller closeAndOpenNextAbove:infocontroller];
            }
            break;
        }
        case NSDownArrowFunctionKey: {
            if (infocontroller.inAnimation) {
                infocontroller.downArrowWhileInAnimation = YES;
                infocontroller.upArrowWhileInAnimation = NO;
            } else {
                [infocontroller.libcontroller closeAndOpenNextBelow:infocontroller];
            }
            break;
        }
        case ' ': {
            if (!infocontroller.inAnimation) {
                [self.window performClose:nil];
            }
            break;
        }

        default: {
            [super keyDown:event];
            break;
        }
    }
}

@end

@interface InfoController () <NSWindowDelegate, NSTextFieldDelegate>
{
    IBOutlet NSTextField *authorField;
    IBOutlet NSTextField *headlineField;
    IBOutlet NSTextField *ifidField;
    IBOutlet NSTextField *descriptionText;
    IBOutlet ImageView *imageView;

    NSWindowController *snapshotController;

    CoreDataManager *coreDataManager;
    NSManagedObjectContext *managedObjectContext;
}
@end

@implementation InfoController

- (instancetype)init {
    self = [super initWithWindowNibName:@"InfoPanel"];
    if (self) {
        coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;
        managedObjectContext = coreDataManager.mainManagedObjectContext;
    }

    return self;
}

- (instancetype)initWithGame:(Game *)game  {
    self = [self init];
    if (self) {
        _game = game;
        _meta = game.metadata;
    }
    return self;
}

// Used for window restoration

- (instancetype)initWithIfid:(NSString *)ifid {
    self = [self init];
    if (self) {
        _game = [TableViewController fetchGameForIFID:ifid inContext:managedObjectContext];
        if (_game) {
            _meta = _game.metadata;
        }
    }
    return self;
}

- (void)windowDidLoad {
    [super windowDidLoad];
    if (@available(macOS 13, *)) {
        self.window.backgroundColor = [NSColor colorNamed:@"customWindowVentura"];
    } else if (@available(macOS 12, *)) {
        self.window.backgroundColor = [NSColor colorNamed:@"customWindowMonterey"];
    } else if (@available(macOS 11, *)) {
        self.window.backgroundColor = [NSColor colorNamed:@"customWindowBigSur"];
    } else {
        self.window.backgroundColor = [NSColor colorNamed:@"customWindowColor"];
    }

    [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:managedObjectContext];

    if (imageView) {
        imageView.game = _game;

        NSImage *image = [[NSImage alloc] initWithData:(NSData *)_meta.cover.data];
        if (!image) {
            image = [NSImage imageNamed:@"Question"];
            imageView.isPlaceholder = YES;
        }
        [imageView processImage:image];
    } else NSLog(@"Error! No imageView!");

    [self update];
    [self sizeToFitImageAnimate:NO];

    _titleField.editable = YES;
    _titleField.delegate = self;

    authorField.editable = YES;
    authorField.delegate = self;

    headlineField.editable = YES;
    headlineField.delegate = self;

    descriptionText.editable = YES;
    descriptionText.delegate = self;

    [self.window makeFirstResponder:self.window.contentView];

    self.window.delegate = self;
}


+ (NSArray *)restorableStateKeyPaths {
    return @[
        @"titleField.stringValue", @"authorField.stringValue",
        @"headlineField.stringValue", @"descriptionText.string"
    ];
}

- (void)sizeToFitImageAnimate:(BOOL)animate {
    if (_inAnimation)
        return;
    NSRect frame;
    NSSize initialWellSize;
    NSSize imgsize;
    NSSize initialWindowSize;
    NSSize setsize;
    NSSize maxsize;
    double scale;

    maxsize = self.window.screen.visibleFrame.size;
    initialWellSize = imageView.frame.size;
    initialWindowSize = self.window.frame.size;

    maxsize.width = maxsize.width * 0.75 - (initialWindowSize.width - initialWellSize.width);
    maxsize.height = maxsize.height * 0.75 - (initialWindowSize.height - initialWellSize.height);

    NSImageRep *rep = imageView.image.representations.lastObject;
    imgsize = NSMakeSize(rep.pixelsWide, rep.pixelsHigh);

    if (imgsize.height == 0)
        return;
    
    CGFloat ratio = (CGFloat)imgsize.width / (CGFloat)imgsize.height;

    if (imgsize.width > maxsize.width) {
        scale = maxsize.width / imgsize.width;
        imgsize.width *= scale;
        imgsize.height *= scale;
    }

    if (imgsize.height > maxsize.height) {
        scale = maxsize.height / imgsize.height;
        imgsize.width *= scale;
        imgsize.height *= scale;
    }

    if (imgsize.width < 100) {
        imgsize.width = 100;
        imgsize.height = 100 / ratio;
    }
    if (imgsize.height < 150) {
        imgsize.height = 150;
        imgsize.width = 150 * ratio;
    }

    setsize.width = initialWindowSize.width - initialWellSize.width + imgsize.width;
    setsize.height = initialWindowSize.height - initialWellSize.height + imgsize.height;

    frame = self.window.frame;
    frame.origin.y += frame.size.height;
    frame.size.width = setsize.width;
    frame.size.height = setsize.height;
    frame.origin.y -= setsize.height;
    if (NSMaxY(frame) > NSMaxY(self.window.screen.visibleFrame))
        frame.origin.y = NSMaxY(self.window.screen.visibleFrame) - frame.size.height;
    if (frame.origin.y < 0)
        frame.origin.y = 0;
    [self.window setFrame:frame display:YES animate:animate];
    [self adjustDescriptionHeight];
    [descriptionText.enclosingScrollView.contentView scrollToPoint:NSZeroPoint];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    if (_inAnimation)
        return;
    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSSet *deletedObjects =  (notification.userInfo)[NSDeletedObjectsKey];
    NSSet *refreshedObjects =  (notification.userInfo)[NSRefreshedObjectsKey];
    if ([deletedObjects containsObject:_game])
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.window performClose:nil];
        });
    if (!updatedObjects)
        updatedObjects = [NSSet new];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];
    if (updatedObjects.count && ([updatedObjects containsObject:_meta] || [updatedObjects containsObject:_game] || [updatedObjects containsObject:_meta.cover])) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self update];
            [self updateImage];
        });
    }
}

- (void)adjustDescriptionHeight {
    NSString *descriptionString = _meta.blurb;
    if (!descriptionString.length)
        descriptionString = descriptionText.stringValue;
    if (!descriptionString.length)
        return;
    NSDictionary *attributes = @{ NSFontAttributeName:[NSFont systemFontOfSize:12] };
    NSRect newFrame = descriptionText.frame;
    NSRect requiredFrame = [descriptionString boundingRectWithSize:NSMakeSize(NSWidth(descriptionText.frame) - 4, MAXFLOAT) options:NSStringDrawingUsesLineFragmentOrigin attributes:attributes context:nil];
    newFrame.size.height = requiredFrame.size.height;
    descriptionText.frame = newFrame;
    descriptionText.stringValue = descriptionString;
}

- (void)update {
    if (_meta.title.length) {
        self.window.title =
        [NSString stringWithFormat:@"%@ Info", _meta.title];
    } else self.window.title = NSLocalizedString(@"Game Info", nil);

    if (_meta) {
        if (_meta.title.length)
            _titleField.stringValue = _meta.title;
        if (_meta.author.length)
            authorField.stringValue = _meta.author;
        if (_meta.headline.length)
            headlineField.stringValue = _meta.headline;
        if (_meta.blurb.length) {
            [self adjustDescriptionHeight];
        }
        if (_game.ifid.length)
            ifidField.stringValue = _game.ifid;
    }
}

- (void)updateImage {
    NSImage *image = nil;
    if (_meta.cover) {
        image = [[NSImage alloc] initWithData:(NSData *)_meta.cover.data];
        imageView.isPlaceholder = NO;
    }

    if (!image) {
        image = [NSImage imageNamed:@"Question"];
        imageView.isPlaceholder = YES;
    }
    [imageView processImage:image];
    [self sizeToFitImageAnimate:YES];
}

- (void)windowWillClose:(NSNotification *)notification {

    // Make sure that all edits are saved
    if (_titleField.stringValue.length && ![_meta.title isEqualToString:_titleField.stringValue])
        _meta.title = _titleField.stringValue;
    if (![_meta.headline isEqualToString:headlineField.stringValue])
        _meta.headline = headlineField.stringValue;
    if (![_meta.author isEqualToString:authorField.stringValue])
        _meta.author = authorField.stringValue;
    if (![_meta.blurb isEqualToString:descriptionText.stringValue])
        _meta.blurb = descriptionText.stringValue;

    // Crazy stuff to make stacks of windows close in a pretty way
    NSArray <InfoController *> *windowArray = _libcontroller.infoWindows.allValues;
    if (windowArray.count > 1) {
        // We look at the title fields of the array of open info window controllers
        // and sort them alphabetically
        windowArray =
        [windowArray sortedArrayUsingComparator:
         ^NSComparisonResult(InfoController * obj1, InfoController * obj2){
            NSString *title1 = obj1.titleField.stringValue;
            NSString *title2 = obj2.titleField.stringValue;
            return [title1 localizedCaseInsensitiveCompare:title2];
        }];
        NSUInteger index = [windowArray indexOfObject:self];
        if (index != NSNotFound) {
            InfoController *next;
            if (index == 0)
                next = windowArray.lastObject;
            else
                next = windowArray[index - 1];
            [next.window makeKeyAndOrderFront:nil];
        }
    }

    [self animateOut];
}

- (void)controlTextDidEndEditing:(NSNotification *)notification
{
    if ([notification.object isKindOfClass:[NSTextField class]])
    {
        NSTextField *textfield = notification.object;

        NSString *trimmedString = [textfield.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (trimmedString.length == 0)
            textfield.stringValue = @"";

        if (textfield == _titleField)
        {
            if (_titleField.stringValue.length && ![_meta.title isEqualToString:_titleField.stringValue]) {
                _meta.title = _titleField.stringValue;
            } else if (_titleField.stringValue.length == 0) {
                _titleField.stringValue = _meta.title;
            }
        }
        else if (textfield == headlineField)
        {
            if (![_meta.headline isEqualToString:headlineField.stringValue])
                _meta.headline = headlineField.stringValue;
        }
        else if (textfield == authorField)
        {
            if (![_meta.author isEqualToString:authorField.stringValue])
                _meta.author = authorField.stringValue;

        }  else if (textfield == descriptionText)
        {
            if (![_meta.blurb isEqualToString:descriptionText.stringValue]) {
                _meta.blurb = descriptionText.stringValue;
                [self adjustDescriptionHeight];
            }
        }

        // When the user has edited text and pressed enter, we want the field to be deselected,
        // but we must check that the user did not end editing by selecting a different field,
        // because then the previous field has already been deselected, and we don't want to deselect
        // the new one.
        dispatch_async(dispatch_get_main_queue(), ^{
            if (textfield == self->descriptionText && self->descriptionText.stringValue.length) {
                // Scroll to bottom
                NSClipView* clipView = textfield.enclosingScrollView.contentView;
                CGFloat newScrollOrigin = NSMaxY(textfield.frame) - NSHeight(clipView.bounds);
                [clipView scrollToPoint:NSMakePoint(0, newScrollOrigin)];
            }
            if (self.window.firstResponder == textfield.currentEditor) {
                [self.window makeFirstResponder:self.window.contentView];
            }
        });
    }
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return managedObjectContext.undoManager;
}

- (void)windowDidResignKey:(NSNotification *)notification {
    if (notification.object == self.window)
        [imageView resignFirstResponder];
}

#pragma mark animation

- (void)makeAndPrepareSnapshotWindow:(NSRect)startingframe {
    CALayer *snapshotLayer = [self takeSnapshot];
    NSWindow *snapshotWindow = [self createFullScreenWindow];
    CALayer *shadowLayer = [self shadowLayerFromFrame:startingframe andWindow:snapshotWindow];
    [snapshotWindow.contentView.layer addSublayer:shadowLayer];
    [snapshotWindow.contentView.layer addSublayer:snapshotLayer];

    snapshotController = [[NSWindowController alloc] initWithWindow:snapshotWindow];
    // Compute the frame of the snapshot layer such that the snapshot is
    // positioned on startingframe.
    NSRect snapshotLayerFrame =
    [snapshotWindow convertRectFromScreen:startingframe];
    snapshotLayer.frame = snapshotLayerFrame;
    [snapshotWindow orderFront:nil];
}

- (CALayer *)takeSnapshot {
    CGImageRef windowSnapshot =
    CGWindowListCreateImage(CGRectNull,
                            kCGWindowListOptionIncludingWindow,
                            (CGWindowID)(self.window).windowNumber,
                            kCGWindowImageBoundsIgnoreFraming);
    CALayer *snapshotLayer = [[CALayer alloc] init];
    snapshotLayer.frame = self.window.frame;
    snapshotLayer.contents = CFBridgingRelease(windowSnapshot);
    snapshotLayer.anchorPoint = CGPointMake(0, 0);
    return snapshotLayer;
}

- (CALayer *)shadowLayerFromFrame:(NSRect)rect andWindow:(NSWindow *)window {
    CALayer *shadowLayer = [CALayer layer];
    CGColorRef shadowColor = CGColorCreateGenericRGB(0, 0, 0, 0.40);
    shadowLayer.shadowColor = shadowColor;
    shadowLayer.shadowOffset = (CGSize){0, -20};
    shadowLayer.shadowRadius = 17;
    shadowLayer.shadowOpacity = 1.0;
    CGColorRelease(shadowColor);

    CGRect windowFrame = [window convertRectFromScreen:rect];
    NSRect shadowRect = [self shadowRectWithRect:windowFrame];

    CGPathRef shadowPath = CGPathCreateWithRect(shadowRect, NULL);
    shadowLayer.shadowPath = shadowPath;
    CGPathRelease(shadowPath);

    [window.contentView.layer addSublayer:shadowLayer];
    return shadowLayer;
}

- (CGRect)shadowRectWithRect:(CGRect)rect {
    NSRect shadowRect = CGRectInset(rect, -2, 0);
    shadowRect.size.height += 5;
    return shadowRect;
}

- (void)animateIn:(NSRect)finalframe {
    NSRect startingFrame = [_libcontroller rectForLineWithIfid:_game.ifid];
    [self makeAndPrepareSnapshotWindow:startingFrame];

    NSWindow *snapshotWindow = snapshotController.window;
    CALayer *shadowLayer = snapshotWindow.contentView.layer.sublayers.firstObject;
    CALayer *snapshotLayer = snapshotWindow.contentView.layer.sublayers[1];

    NSRect finalLayerFrame = [snapshotWindow convertRectFromScreen:finalframe];
    snapshotLayer.frame = [snapshotWindow convertRectFromScreen:startingFrame];

    // We need to explicitly animate the shadow path to reflect the new size.
    CGPathRef shadowPath = CGPathCreateWithRect([self shadowRectWithRect:finalLayerFrame], NULL);
    CABasicAnimation *shadowAnimation = [CABasicAnimation animationWithKeyPath:@"shadowPath"];
    shadowAnimation.fromValue = (id)shadowLayer.shadowPath;
    shadowAnimation.toValue = (__bridge id)(shadowPath);
    shadowAnimation.duration = .3;
    shadowAnimation.removedOnCompletion = NO;
    shadowAnimation.fillMode = kCAFillModeForwards;

    CABasicAnimation *transformAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    transformAnimation.duration=.3;
    CGFloat scaleFactorX = NSWidth(finalLayerFrame) / NSWidth(snapshotLayer.frame);
    CGFloat scaleFactorY = NSHeight(finalLayerFrame) / NSHeight(snapshotLayer.frame);
    transformAnimation.toValue=[NSValue valueWithCATransform3D:CATransform3DMakeScale(scaleFactorX, scaleFactorY, 1)];
    transformAnimation.removedOnCompletion = NO;
    transformAnimation.fillMode = kCAFillModeForwards;

    // Prepare the animation from the current position to the new position
    CABasicAnimation *positionAnimation = [CABasicAnimation animationWithKeyPath:@"position"];
    positionAnimation.fromValue = [snapshotLayer valueForKey:@"position"];
    NSPoint point = finalLayerFrame.origin;
    positionAnimation.toValue = [NSValue valueWithPoint:point];
    positionAnimation.fillMode = kCAFillModeForwards;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = .3;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

        shadowLayer.shadowPath = shadowPath;
        CGPathRelease(shadowPath);
        [shadowLayer addAnimation:shadowAnimation forKey:@"shadowPath"];

        snapshotLayer.position = point;
        [snapshotLayer addAnimation:positionAnimation forKey:@"position"];
        [snapshotLayer addAnimation:transformAnimation forKey:@"transform"];
    } completionHandler:^{
        self.window.alphaValue = 1.0;
        [self.window setFrame:finalframe display:YES];
        snapshotWindow.contentView.hidden = YES;
        [self->snapshotController close];
        self.inAnimation = NO;
        [self checkForKeyPressesDuringAnimation];
    }];
}

- (void)animateOut {
    _inAnimation = YES;

    [self makeAndPrepareSnapshotWindow:self.window.frame];
    NSWindow *snapshotWindow = snapshotController.window;
    NSView *snapshotView = snapshotWindow.contentView;
    CALayer *shadowLayer = snapshotView.layer.sublayers.firstObject;
    CALayer *snapshotLayer = snapshotView.layer.sublayers[1];

    TableViewController *libctrl = _libcontroller;

    if (_libcontroller == nil)
        NSLog(@"nil!");

    NSRect currentFrame = snapshotLayer.frame;
    NSRect targetFrame = [libctrl rectForLineWithIfid:_game.ifid];

    NSRect finalLayerFrame = [snapshotWindow convertRectFromScreen:targetFrame];

    CABasicAnimation *transformAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    transformAnimation.duration=.4;
    CGFloat scaleFactorX = NSWidth(finalLayerFrame) / NSWidth(currentFrame);
    CGFloat scaleFactorY = NSHeight(finalLayerFrame) / NSHeight(currentFrame);
    transformAnimation.toValue=[NSValue valueWithCATransform3D:CATransform3DMakeScale(scaleFactorX, scaleFactorY, 1)];
    transformAnimation.removedOnCompletion = NO;
    transformAnimation.fillMode = kCAFillModeForwards;

// We need to explicitly animate the shadow path to reflect the new size.
    CGPathRef shadowPath = CGPathCreateWithRect([self shadowRectWithRect:finalLayerFrame], NULL);
    CABasicAnimation *shadowAnimation = [CABasicAnimation animationWithKeyPath:@"shadowPath"];
    shadowAnimation.fromValue = (id)shadowLayer.shadowPath;
    shadowAnimation.toValue = (__bridge id)(shadowPath);
    shadowAnimation.duration = .4;
    shadowAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
    shadowAnimation.removedOnCompletion = NO;
    shadowAnimation.fillMode = kCAFillModeForwards;

    CABasicAnimation *positionAnimation = [CABasicAnimation animationWithKeyPath:@"position"];
    positionAnimation.fromValue = [snapshotLayer valueForKey:@"position"];
    NSPoint point = finalLayerFrame.origin;
    positionAnimation.toValue = [NSValue valueWithPoint:point];
    positionAnimation.fillMode = kCAFillModeForwards;

    NSString *ifid = _game.ifid;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = .4;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
        shadowLayer.shadowPath = shadowPath;
        CGPathRelease(shadowPath);
        [shadowLayer addAnimation:shadowAnimation forKey:@"shadowPath"];
        snapshotLayer.position = point;
        [snapshotLayer addAnimation:positionAnimation forKey:@"position"];
        [snapshotLayer addAnimation:transformAnimation forKey:@"transform"];
    }
     completionHandler:^{
        self.inAnimation = NO;
        snapshotView.hidden = YES;
        [self->snapshotController close];
        self->snapshotController = nil;

        [self checkForKeyPressesDuringAnimation];

        [libctrl.infoWindows removeObjectForKey:ifid];
    }];
}

+ (CABasicAnimation *)fadeOutAnimation {
    CABasicAnimation *fadeOutAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeOutAnimation.fromValue = @1.0f;
    fadeOutAnimation.toValue = @0.0f;
    fadeOutAnimation.additive = NO;
    fadeOutAnimation.removedOnCompletion = NO;
    fadeOutAnimation.beginTime = 0.0;
    fadeOutAnimation.duration = .2;
    fadeOutAnimation.fillMode = kCAFillModeForwards;
    return fadeOutAnimation;
}

+ (void)closeStrayInfoWindows {
    NSArray *windows = [NSApplication sharedApplication].windows;
    for (NSWindow *window in windows)
        if ([window isKindOfClass:[InfoPanel class]] && ![window.delegate isKindOfClass:[InfoController class]])
            [window close];
}

- (void)checkForKeyPressesDuringAnimation {
    if (_downArrowWhileInAnimation) {
        [_libcontroller closeAndOpenNextBelow:self];
    } else if (_upArrowWhileInAnimation) {
        [_libcontroller closeAndOpenNextAbove:self];
    }
    _downArrowWhileInAnimation = NO;
    _upArrowWhileInAnimation = NO;
}

- (void)hideWindow {
    _inAnimation = YES;
    // So we need to get a screenshot of the window without flashing.
    // First, we find the frame that covers all the connected screens.
    CGRect allWindowsFrame = CGRectZero;

    for(NSScreen *screen in [NSScreen screens]) {
        allWindowsFrame = NSUnionRect(allWindowsFrame, screen.frame);
    }

    // Position our window to the very right-most corner out of visible range, plus padding for the shadow.
    CGRect frame = (CGRect){
        .origin = CGPointMake(CGRectGetWidth(allWindowsFrame) + 2 * 19.f, 0),
        .size = self.window.frame.size
    };

    // This is where things get nasty. Against what the documentation states, windows seem to be constrained
    // to the screen, so we override "constrainFrameRect:toScreen:" to return the original frame, which allows
    // us to put the window off-screen.
    ((InfoPanel *)self.window).disableConstrainedWindow = YES;

    [self.window setFrame:frame display:YES];
    [self showWindow:nil];
}

- (NSWindow *)createFullScreenWindow {
    NSWindow *fullScreenWindow =
    [[NSWindow alloc] initWithContentRect:(CGRect){ .size = _libcontroller.view.window.screen.frame.size }
                                styleMask:NSWindowStyleMaskBorderless
                                  backing:NSBackingStoreBuffered
                                    defer:NO
                                   screen:_libcontroller.view.window.screen];
    fullScreenWindow.animationBehavior = NSWindowAnimationBehaviorNone;
    fullScreenWindow.backgroundColor = NSColor.clearColor;
    fullScreenWindow.movableByWindowBackground = NO;
    fullScreenWindow.ignoresMouseEvents = YES;
    fullScreenWindow.level = _libcontroller.view.window.level;
    fullScreenWindow.hasShadow = NO;
    fullScreenWindow.opaque = NO;
    NSView *contentView = [[NSView alloc] initWithFrame:NSZeroRect];
    contentView.wantsLayer = YES;
    contentView.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
    fullScreenWindow.contentView = contentView;
    return fullScreenWindow;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    return [self.libcontroller validateMenuItem:menuItem];
}

- (IBAction)like:(id)sender {
    return [self.libcontroller like:sender];
}
- (IBAction)dislike:(id)sender {
    return [self.libcontroller dislike:sender];
}
- (IBAction)play:(id)sender {
    return [self.libcontroller play:sender];
}
- (IBAction)download:(id)sender {
    return [self.libcontroller download:sender];
}
- (IBAction)revealGameInFinder:(id)sender {
    return [self.libcontroller revealGameInFinder:sender];
}
- (IBAction)deleteGame:(id)sender {
    return [self.libcontroller deleteGame:sender];
}
- (IBAction)selectSameTheme:(id)sender {
    return [self.libcontroller selectSameTheme:sender];
}
- (IBAction)deleteSaves:(id)sender {
    return [self.libcontroller deleteSaves:sender];
}
- (IBAction)openIfdb:(id)sender {
    return [self.libcontroller openIfdb:sender];
}
- (IBAction)applyTheme:(id)sender {
    return [self.libcontroller applyTheme:sender];
}

@end

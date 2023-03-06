//
//  PreviewViewController.h
//  SpatterlightQuickLook
//
//  Created by Administrator on 2021-01-29.
//

#import <Cocoa/Cocoa.h>

@class NSPersistentContainer, NonInterpolatedImage;

API_AVAILABLE(macos(10.12))
@interface PreviewViewController : NSViewController

@property BOOL showingIcon;
@property BOOL addedFileInfo;
@property BOOL vertical;
@property BOOL showingView;

@property NSString *ifid;

@property (readonly) NSPersistentContainer *persistentContainer;

@property (strong) IBOutlet NSTextView *textview;
@property (strong) IBOutlet NSScrollView *textScrollView;

@property (strong) IBOutlet NonInterpolatedImage *imageView;
@property (strong) IBOutlet NSView *imageBox;

@property (strong) NSLayoutConstraint *imageRatio;

@property (strong) IBOutlet NSLayoutConstraint *imageBottomToContainerBottom;
@property (strong) IBOutlet NSLayoutConstraint *imageBoxHeightTracksTextBox;
@property (strong) IBOutlet NSLayoutConstraint *imageTopToContainerTop;
@property (strong) IBOutlet NSLayoutConstraint *imagePinnedToContainerBottom;
@property (strong) IBOutlet NSLayoutConstraint *imageTrailingToCenterX;
@property (strong) IBOutlet NSLayoutConstraint *textBottomToContainerBottom;
@property (strong) IBOutlet NSLayoutConstraint *textTopToContainerTop;
@property (strong) IBOutlet NSLayoutConstraint *centerImageVertically;
@property (strong) IBOutlet NSLayoutConstraint *textLeadingTracksImageTrailing;
@property (strong) IBOutlet NSLayoutConstraint *textClipHeight;
@property (strong) IBOutlet NSLayoutConstraint *imageWidthConstraint;

@end

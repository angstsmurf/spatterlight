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

@property (unsafe_unretained) IBOutlet NSTextView *textview;

@property (weak) IBOutlet NonInterpolatedImage *imageView;

@property (weak) IBOutlet NSLayoutConstraint *imageCenterYtoContainerCenter;
@property (weak) IBOutlet NSLayoutConstraint *textBottomToContainerBottom;
@property (weak) IBOutlet NSLayoutConstraint *textTopToContainerTop;

@property (weak) IBOutlet NSLayoutConstraint *imageTrailingToCenterX;
@property (weak) IBOutlet NSLayoutConstraint *textWidthProportionalToContainer;
@property (weak) IBOutlet NSLayoutConstraint *textLeadingTracksImageTrailing;

@property NSLayoutConstraint *textClipHeight;
@property NSLayoutConstraint *imageTopEqualsTextTop;
@property NSLayoutConstraint *imageHeightTracksImageWidth;

@end

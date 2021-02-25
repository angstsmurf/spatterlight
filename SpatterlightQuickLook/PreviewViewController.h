//
//  PreviewViewController.h
//  SpatterlightQuickLook
//
//  Created by Administrator on 2021-01-29.
//

#import <Cocoa/Cocoa.h>

@class NSPersistentContainer, MyTextView;

API_AVAILABLE(macos(10.12))
@interface PreviewViewController : NSViewController {
    NSMutableArray *ifidbuf;
    NSMutableDictionary *metabuf;
}

@property NSSize originalImageSize;
@property CGFloat preferredWidth;
@property BOOL showingIcon;
@property BOOL addedFileInfo;
@property BOOL vertical;
@property BOOL showingView;

@property NSUInteger hasSized;
@property NSString *ifid;

@property (readonly) NSPersistentContainer *persistentContainer;

@property (weak) NSString *string;
@property (unsafe_unretained) IBOutlet NSTextView *textview;

@property (weak) IBOutlet NSImageView *imageView;

@end

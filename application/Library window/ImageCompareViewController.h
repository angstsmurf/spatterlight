//
//  CoverImageComparisonViewController.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-21.
//

#import <Cocoa/Cocoa.h>

typedef NS_ENUM(NSUInteger, kImageComparisonSource) {
    kImageComparisonDownloaded,
    kImageComparisonLocalFile
};

typedef NS_ENUM(NSUInteger, kImageComparisonResult ) {
    kImageComparisonResultA,
    kImageComparisonResultB,
    kImageComparisonResultWantsUserInput,
};

NS_ASSUME_NONNULL_BEGIN

@interface ImageCompareViewController : NSViewController

@property (weak) IBOutlet NSImageView *leftImage;
@property (weak) IBOutlet NSImageView *rightImage;

@property (weak) IBOutlet NSButton *imageSelectDialogSuppressionButton;

+ (kImageComparisonResult)chooseImageA:(NSData *)imageA orB:(NSData *)imageB source:(kImageComparisonSource)source force:(BOOL)force;

- (BOOL)userWantsImage:(NSData *)imageA ratherThanImage:(NSData *)imageB source:(kImageComparisonSource)type force:(BOOL)force;

@end

NS_ASSUME_NONNULL_END

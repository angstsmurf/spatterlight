//
//  CoverImageComparisonViewController.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-21.
//

#import <Cocoa/Cocoa.h>

typedef enum kImageComparisonType : NSUInteger {
    DOWNLOADED,
    LOCAL,
} kImageComparisonType;

NS_ASSUME_NONNULL_BEGIN

@interface ImageCompareViewController : NSViewController

@property (weak) IBOutlet NSImageView *leftImage;
@property (weak) IBOutlet NSImageView *rightImage;

@property (weak) IBOutlet NSButton *imageSelectDialogSuppressionButton;

- (BOOL)userWantsImage:(NSData *)imageA ratherThanImage:(NSData *)imageB type:(kImageComparisonType)type;

@end

NS_ASSUME_NONNULL_END

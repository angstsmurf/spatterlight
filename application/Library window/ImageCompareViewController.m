//
//  CoverImageComparisonViewController.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-21.
//

#import "NSData+MD5.h"
#import "ImageCompareViewController.h"


@interface ImageCompareViewController ()

@end

@implementation ImageCompareViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do view setup here.
}

- (BOOL)userWantsImage:(NSData *)imageA ratherThanImage:(NSData *)imageB {

    if (!imageB.length || [imageB isPlaceHolderImage])
        return YES;
    if (!imageA.length || [imageA isPlaceHolderImage])
        return NO;
    if ([imageA isEqual:imageB]) {
        NSLog(@"The images are the same");
        return NO;
    }

    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:NSLocalizedString(@"Do you want the replace this image?", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Okay", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    alert.accessoryView = self.view;
    _rightImage.image = [[NSImage alloc] initWithData:imageA];
    _leftImage.image = [[NSImage alloc] initWithData:imageB];
    NSModalResponse choice = [alert runModal];

    return (choice == NSAlertFirstButtonReturn);
}


@end

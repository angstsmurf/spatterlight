//
//  ImageCompareViewController.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-21.
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"


#import "NSData+Categories.h"
#import "NSImage+Categories.h"

#import "ImageCompareViewController.h"
#import "Preferences.h"

#import "ImageSimilarity.h"
#import "Constants.h"

@interface ImageCompareViewController ()

@property (weak) IBOutlet NSImageView *leftImage;
@property (weak) IBOutlet NSImageView *rightImage;

@property (weak) IBOutlet NSButton *imageSelectDialogSuppressionButton;

@end

@implementation ImageCompareViewController

+ (kImageComparisonResult)chooseImageA:(NSData *)imageA orB:(nullable NSData *)imageB source:(kImageComparisonSource)source force:(BOOL)force {
    if (!imageB.length || [imageB isPlaceHolderImage])
        return kImageComparisonResultA;
    if (!imageA.length || [imageA isPlaceHolderImage])
        return kImageComparisonResultB;

    if ([ImageSimilarity imageData:imageA looksLike:imageB]) {
        NSLog(@"The images are the same");
        return kImageComparisonResultB;
    }

    if (!force) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        kImageReplacementPrefsType userSetting = (kImageReplacementPrefsType)[defaults integerForKey:kImageReplacementKey];
        if (source == kImageComparisonDownloaded) {
            if (userSetting == kNeverReplace)
                return kImageComparisonResultB;
            if (userSetting == kAlwaysReplace)
                return kImageComparisonResultA;
        } else if (source == kImageComparisonLocalFile && [defaults boolForKey:kImageComparisonSuppressionKey]) {
            return kImageComparisonResultA;
        }
    }
    return kImageComparisonResultWantsUserInput;
}

- (BOOL)userWantsImage:(NSData *)imageA ratherThanImage:(NSData *)imageB source:(kImageComparisonSource)source force:(BOOL)force {

    kImageComparisonResult result = [ImageCompareViewController chooseImageA:imageA orB:imageB source:source force:force];

    if (result == kImageComparisonResultA)
        return YES;
    if (result == kImageComparisonResultB)
        return NO;

    NSModalResponse choice = [self showAlertWithImageA:(NSData *)imageA imageB:(NSData *)imageB source:source];

    return (choice == NSAlertFirstButtonReturn);
}

- (NSModalResponse)showAlertWithImageA:(NSData *)imageA imageB:(NSData *)imageB source:(kImageComparisonSource)source {
    NSAlert *alert = [[NSAlert alloc] init];

    alert.accessoryView = self.view;
    _rightImage.image = [[NSImage alloc] initWithData:imageA];
    _leftImage.image = [[NSImage alloc] initWithData:imageB];

    [alert setMessageText:NSLocalizedString(@"Do you want to replace this image?", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"No", nil)];

    NSButton *checkbox = _imageSelectDialogSuppressionButton;
    if (source == kImageComparisonLocalFile) {
        [alert layout];
        checkbox.title = NSLocalizedString(@"Don't ask again", nil);
        NSRect frame = checkbox.frame;
        frame.origin.x += 25;
        checkbox.frame = frame;
    }

    NSModalResponse choice = [alert runModal];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if (checkbox.state == NSControlStateValueOn) {
        if (source == kImageComparisonDownloaded) {
            [defaults setInteger:(choice == NSAlertFirstButtonReturn) ? kAlwaysReplace : kNeverReplace forKey:kImageReplacementKey];
            if (Preferences.instance)
                [Preferences.instance updatePrefsPanel];
        } else {
            [defaults setBool:YES forKey: kImageComparisonSuppressionKey];
        }
    }
    return choice;
}

@end

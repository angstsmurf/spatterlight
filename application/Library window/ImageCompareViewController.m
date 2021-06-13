//
//  CoverImageComparisonViewController.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-21.
//

#import "NSData+Categories.h"
#import "NSView+snapshot.h"
#import "NSImage+Categories.h"

#import "ImageCompareViewController.h"
#import "Preferences.h"

#import "OSImageHashing.h"

@interface ImageCompareViewController ()

@end

@implementation ImageCompareViewController

- (BOOL)userWantsImage:(NSData *)imageA ratherThanImage:(NSData *)imageB {

    if (!imageB.length || [imageB isPlaceHolderImage])
        return YES;
    if (!imageA.length || [imageA isPlaceHolderImage])
        return NO;

    if([imageA isEqualToData:imageB]) {
        NSLog(@"The images are the same");
        return NO;
    }

    if([[OSImageHashing sharedInstance] compareImageData:imageA to:imageB]) {
        NSLog(@"The images are the same according to OSImageHashing");
        return YES;
    }

    if ([[NSUserDefaults standardUserDefaults] integerForKey:@"ImageReplacement"] == kNeverReplace)
        return NO;

    if ([[NSUserDefaults standardUserDefaults] integerForKey:@"ImageReplacement"] == kAlwaysReplace)
        return YES;

    NSAlert *alert = [[NSAlert alloc] init];
    alert.accessoryView = self.view;
    _rightImage.image = [[NSImage alloc] initWithData:imageA];
    _leftImage.image = [[NSImage alloc] initWithData:imageB];

    [alert setMessageText:NSLocalizedString(@"Do you want the replace this image?", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"No", nil)];

    NSModalResponse choice = [alert runModal];

    if (_imageSelectDialogSuppressionButton.state == NSOnState) {
        [[NSUserDefaults standardUserDefaults] setInteger:(choice == NSAlertFirstButtonReturn) ? kAlwaysReplace : kNeverReplace forKey:@"ImageReplacement"];
        if (Preferences.instance)
            [Preferences.instance updatePrefsPanel];
    }
    return (choice == NSAlertFirstButtonReturn);
}

- (NSData *)pngDataFromRep:(NSBitmapImageRep *)rep {
    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[rep] usingType:NSBitmapImageFileTypePNG properties:props];
}


@end

//
//  CoverImageComparisonViewController.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-21.
//

#import "NSData+Categories.h"
#import "NSImage+Categories.h"

#import "ImageCompareViewController.h"
#import "Preferences.h"

#import "OSImageHashing.h"

@interface ImageCompareViewController ()

@end

@implementation ImageCompareViewController

- (BOOL)userWantsImage:(NSData *)imageA ratherThanImage:(NSData *)imageB type:(kImageComparisonType)type {

    if (!imageB.length || [imageB isPlaceHolderImage])
        return YES;
    if (!imageA.length || [imageA isPlaceHolderImage])
        return NO;

    if([imageA isEqualToData:imageB]) {
        NSLog(@"The images are the same");
        return NO;
    }

    if ([[OSImageHashing sharedInstance] compareImageData:imageA to:imageB]) {
        NSLog(@"The images are the same according to OSImageHashing");
        return NO;
    }

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    if (type == DOWNLOADED) {
        if ([defaults integerForKey:@"ImageReplacement"] == kNeverReplace)
            return NO;
        
        if ([defaults integerForKey:@"ImageReplacement"] == kAlwaysReplace)
            return YES;
    } else if (type == LOCAL && [defaults boolForKey:@"ImageComparisonSuppression"]) {
        return YES;
    }

    NSAlert *alert = [[NSAlert alloc] init];

    alert.accessoryView = self.view;
    _rightImage.image = [[NSImage alloc] initWithData:imageA];
    _leftImage.image = [[NSImage alloc] initWithData:imageB];

    [alert setMessageText:NSLocalizedString(@"Do you want the replace this image?", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Yes", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"No", nil)];

    if (type == LOCAL) {
        [alert layout];
        _imageSelectDialogSuppressionButton.title = NSLocalizedString(@"Don't ask again", nil);
        NSRect frame = _imageSelectDialogSuppressionButton.frame;
        frame.origin.x += 25;
        _imageSelectDialogSuppressionButton.frame = frame;
    }

    NSModalResponse choice = [alert runModal];

    if (_imageSelectDialogSuppressionButton.state == NSOnState) {
        if (type == DOWNLOADED) {
            [defaults setInteger:(choice == NSAlertFirstButtonReturn) ? kAlwaysReplace : kNeverReplace forKey:@"ImageReplacement"];
            if (Preferences.instance)
                [Preferences.instance updatePrefsPanel];
        } else {
            [defaults setBool:YES forKey: @"ImageComparisonSuppression"];
        }
    }

    return (choice == NSAlertFirstButtonReturn);
}

- (NSData *)pngDataFromRep:(NSBitmapImageRep *)rep {
    NSDictionary *props = @{ NSImageInterlaced: @(NO) };
    return [NSBitmapImageRep representationOfImageRepsInArray:@[rep] usingType:NSBitmapImageFileTypePNG properties:props];
}


@end

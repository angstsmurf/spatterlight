//
//  ComparisonOperation.m
//
//  Created by Administrator on 2021-10-21.
//

#import "ComparisonOperation.h"

#import "ImageCompareViewController.h"


@interface ComparisonOperation ()

@property NSData *imageData;
@property NSData *oldImageData;

@property (nonatomic, copy, nullable) void (^completionHandler)(void);
@property BOOL force;

@end


@implementation ComparisonOperation

- (instancetype)initWithNewData:(NSData *)newData oldData:(NSData *)oldData force:(BOOL)force completionHandler:(void (^)(void))completionHandler {
    if ((self = super.init)) {
        _imageData = newData;
        _oldImageData = oldData;
        _force = force;
        _completionHandler = completionHandler;
    }
    return self;
}

- (BOOL)isAsynchronous {
    return NO;
}

- (void)main {
    if (self.cancelled) {
        NSLog(@"** operation cancelled **");
        return;
    }

    NSData *dataA = _imageData;
    NSData *dataB = _oldImageData;
    BOOL force = _force;
    void (^completionHandler)(void) = _completionHandler;

    NSLog(@"Running ComparisonOperation main!");

    dispatch_async(dispatch_get_main_queue(), ^{
        ImageCompareViewController *imageCompare = [[ImageCompareViewController alloc] initWithNibName:@"ImageCompareViewController" bundle:nil];
        if ([imageCompare userWantsImage:dataA ratherThanImage:dataB source:kImageComparisonDownloaded force:force]) {
            completionHandler();
        } 
    });
}

@end

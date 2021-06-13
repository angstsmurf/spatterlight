//
//  NSView+snapshot.m
//  DragDrop
//
//  Created by Administrator on 2021-06-04.
//

#import "NSView+snapshot.h"

@implementation NSView (snapshot)

- (NSBitmapImageRep *)snapshotRep {
    NSBitmapImageRep *bitmap = [self bitmapImageRepForCachingDisplayInRect:self.bounds];
    [self cacheDisplayInRect:self.bounds toBitmapImageRep:bitmap];
    return bitmap;
}

- (NSImage *)snapshot {
    NSBitmapImageRep *bitmap = [self snapshotRep];
    NSImage *image = [[NSImage alloc] initWithSize:self.bounds.size];
    [image addRepresentation:bitmap];
    return image;
}

@end

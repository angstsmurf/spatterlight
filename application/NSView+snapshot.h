//
//  NSView+snapshot.h
//  DragDrop
//
//  Created by Administrator on 2021-06-04.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSView (snapshot)

- (NSImage *)snapshot;
- (NSBitmapImageRep *)snapshotRep;

@end

NS_ASSUME_NONNULL_END

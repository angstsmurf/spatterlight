//
//  MyFilePromiseProvider.m
//  Spatterlight
//
//  Created by Administrator on 2021-06-09.
//

#import "MyFilePromiseProvider.h"

#import "ImageDragSource.h"

@implementation MyFilePromiseProvider

- (NSArray<NSPasteboardType> *)writableTypesForPasteboard:(NSPasteboard *)pasteboard {
    NSMutableArray<NSPasteboardType> *types = [super writableTypesForPasteboard:pasteboard].mutableCopy;
    // A file URL too, for receivers that don't take file promises (Terminal).
    NSArray *myTypes = @[NSPasteboardTypePNG, NSPasteboardTypeFileURL];

    [types addObjectsFromArray:myTypes];

    return types;
}

- (NSPasteboardWritingOptions)writingOptionsForType:(NSPasteboardType)type pasteboard: (NSPasteboard *)pasteboard {
    if ([type isEqual: NSPasteboardTypePNG]) {
        return 0;
    }
    // Only write the file if a receiver asks for it.
    if ([type isEqual: NSPasteboardTypeFileURL]) {
        return NSPasteboardWritingPromised;
    }
    return [super writingOptionsForType: type pasteboard: pasteboard];
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {

    if ([type isEqual: NSPasteboardTypePNG]) {
        return ((ImageDragSource *)self.delegate).pngData;
    }
    if ([type isEqual: NSPasteboardTypeFileURL]) {
        return [((ImageDragSource *)self.delegate).temporaryFileURL pasteboardPropertyListForType:NSPasteboardTypeFileURL];
    }

    return [super pasteboardPropertyListForType: type];
}

@end

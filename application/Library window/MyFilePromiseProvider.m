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
    NSArray *myTypes = @[NSPasteboardTypePNG];

    [types addObjectsFromArray:myTypes];

    return types;
}

- (NSPasteboardWritingOptions)writingOptionsForType:(NSPasteboardType)type pasteboard: (NSPasteboard *)pasteboard {
    if ([type isEqual: NSPasteboardTypePNG]) {
        return 0;
    }
    return [super writingOptionsForType: type pasteboard: pasteboard];
}

- (id)pasteboardPropertyListForType:(NSPasteboardType)type {

    if ([type isEqual: NSPasteboardTypePNG]) {
        return ((ImageDragSource *)self.delegate).pngData;
    }

    return [super pasteboardPropertyListForType: type];
}

@end

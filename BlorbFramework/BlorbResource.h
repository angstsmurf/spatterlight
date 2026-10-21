//
//  BlorbResource.h
//  Spatterlight
//
//  Based on code from Yazmin by David Schweinsberg
//  https://github.com/yazmin
//
//  Original code created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Foundation/Foundation.h>

// The usage field of a Blorb resource index entry.
typedef NS_ENUM(FourCharCode, BlorbUsage) {
    BlorbUsageExecutable = 0x45786563, // 'Exec'
    BlorbUsagePicture = 0x50696374,    // 'Pict'
    BlorbUsageSound = 0x536e6420       // 'Snd '
};

NS_ASSUME_NONNULL_BEGIN

// One entry of the resource index, plus what the rest of the file says
// about it.
@interface BlorbResource : NSObject

@property(readonly) FourCharCode usage;
@property(readonly) uint32_t number;
// Offset of the resource's chunk header in the file.
@property(readonly) uint32_t start;
// From the RDes chunk.
@property(copy, nullable) NSString *descriptionText;
// The chunk ID at start, e.g. "ZCOD" or "PNG ", or nil if start is past
// the end of the file.
@property(copy, nullable) NSString *chunkType;

- (instancetype)initWithUsage:(FourCharCode)usage
                       number:(uint32_t)number
                        start:(uint32_t)start NS_DESIGNATED_INITIALIZER;
- (instancetype)init __attribute__((unavailable));

@end

NS_ASSUME_NONNULL_END

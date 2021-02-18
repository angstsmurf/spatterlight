//
//  BlorbResource.h
//  Yazmin
//
//  Created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#define ExecutableResource 0x45786563
#define PictureResource 0x50696374
#define SoundResource 0x536e6420

NS_ASSUME_NONNULL_BEGIN

@interface BlorbResource : NSObject

@property(readonly) unsigned int usage;
@property(readonly) unsigned int number;
@property(readonly) unsigned int start;
@property(nullable) NSString *descriptiontext;

- (instancetype)initWithUsage:(unsigned int)usage
                       number:(unsigned int)number
                        start:(unsigned int)start NS_DESIGNATED_INITIALIZER;
- (instancetype)init __attribute__((unavailable));

@end

NS_ASSUME_NONNULL_END

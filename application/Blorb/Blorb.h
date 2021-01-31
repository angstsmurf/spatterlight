//
//  Blorb.h
//  Yazmin
//
//  Created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class BlorbResource;

@interface Blorb : NSObject

@property(readonly) NSArray<BlorbResource *> *resources;
@property(readonly, nullable) NSData *zcodeData;
@property(readonly, nullable) NSData *pictureData;
@property(readonly, nullable) NSData *metaData;

+ (BOOL)isBlorbURL:(NSURL *)url;
+ (BOOL)isBlorbData:(NSData *)data;

- (instancetype)initWithData:(NSData *)data NS_DESIGNATED_INITIALIZER;
- (instancetype)init __attribute__((unavailable));

- (nullable NSData *)dataForResource:(BlorbResource *)resource;
- (NSArray<BlorbResource *> *)resourcesForUsage:(unsigned int)usage;

@end

NS_ASSUME_NONNULL_END

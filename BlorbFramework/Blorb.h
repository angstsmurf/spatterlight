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

@property(readonly, nullable) NSString *snam;

@property NSUInteger releaseNumber;
@property NSUInteger checkSum;
@property(nullable) NSString *serialNumber;

@property(nullable) NSMutableDictionary *optionalChunks;
@property BOOL fakeFrontispiece;

+ (BOOL)isBlorbURL:(nullable NSURL *)url;
+ (BOOL)isBlorbData:(NSData *)data;

- (instancetype)initWithData:(nullable NSData *)data NS_DESIGNATED_INITIALIZER;
- (instancetype)init __attribute__((unavailable));

- (nullable NSData *)dataForResource:(BlorbResource *)resource;
- (NSArray<BlorbResource *> *)resourcesForUsage:(FourCharCode)usage;
- (nullable BlorbResource *)findResourceOfUsage:(FourCharCode)usage;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSData * _Nullable coverImageData;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSString * _Nullable ifidFromIFhd;

@end

NS_ASSUME_NONNULL_END

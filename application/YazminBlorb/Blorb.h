//
//  Blorb.h
//  Yazmin
//
//  Created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@class BlorbResource;

@interface Blorb : NSObject

@property(readonly) NSArray *resources;
@property(readonly) NSData *zcodeData;
@property(readonly) NSData *pictureData;
@property(readonly) NSData *metaData;

+ (BOOL)isBlorbURL:(NSURL *)url;
+ (BOOL)isBlorbData:(NSData *)data;

- (instancetype)initWithData:(NSData *)data;
- (instancetype)init __attribute__((unavailable));

- (NSData *)dataForResource:(BlorbResource *)resource;
- (NSArray *)resourcesForUsage:(unsigned int)usage;

@end

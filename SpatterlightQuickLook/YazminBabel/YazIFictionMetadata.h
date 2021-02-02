//
//  IFictionMetadata.h
//  Yazmin
//
//  Created by David Schweinsberg on 22/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class IFStory;

NS_ASSUME_NONNULL_BEGIN

@interface IFictionMetadata : NSObject

@property(readonly) NSArray<IFStory *> *stories;
@property(readonly) NSString *xmlString;

- (instancetype)initWithData:(NSData *)data NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStories:(NSArray<IFStory *> *)stories
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init __attribute__((unavailable));

- (nullable IFStory *)storyWithIFID:(NSString *)ifid;

@end

NS_ASSUME_NONNULL_END

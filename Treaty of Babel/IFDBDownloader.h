//
//  IFDBDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>

@class Metadata, Image, Game;

@interface IFDBDownloader : NSObject

- (instancetype)initWithContext:(NSManagedObjectContext *)context;
- (BOOL)downloadMetadataFor:(Game*)game imageOnly:(BOOL)imageOnly;
- (BOOL)downloadImageFor:(Metadata *)metadata;
- (Image *)fetchImageForURL:(NSString *)imgurl;
- (Image *)insertImageData:(NSData *)data inMetadata:(Metadata *)metadata;

@property (strong) NSManagedObjectContext *context;

@end

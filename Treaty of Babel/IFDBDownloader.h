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
- (BOOL)downloadMetadataForIFID:(NSString*)ifid;
- (BOOL)downloadMetadataForTUID:(NSString*)tuid;
- (BOOL)downloadMetadataFor:(Game*)game;
- (BOOL)downloadImageFor:(Metadata *)metadata;
- (Image *)fetchImageForURL:(NSString *)imgurl;
- (Image *)insertImageData:(NSData *)data inMetadata:(Metadata *)metadata;

@property (strong) NSManagedObjectContext *context;

@end

//
//  IFDBDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>

@class Metadata, Image;

@interface IFDBDownloader : NSObject

- (instancetype)initWithContext:(NSManagedObjectContext *)context;
- (BOOL)downloadMetadataForIFID:(NSString*)ifid;
- (BOOL)downloadMetadataForTUID:(NSString*)tuid;
- (BOOL)downloadImageFor:(Metadata *)metadata;
- (Image *)fetchImageForURL:(NSString *)imgurl;
- (Image *)insertImage:(NSData *)data inMetadata:(Metadata *)metadata;

@property (strong) NSManagedObjectContext *context;

@end

//
//  IFDBDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>

@class Metadata;

@interface IFDBDownloader : NSObject

- (instancetype)initWithContext:(NSManagedObjectContext *)context;
- (BOOL)downloadMetadataForIFID:(NSString*)ifid;
- (BOOL)downloadMetadataForTUID:(NSString*)tuid;
- (BOOL)downloadImageFor:(Metadata *)metadata;

@property (strong) NSManagedObjectContext *context;

@end

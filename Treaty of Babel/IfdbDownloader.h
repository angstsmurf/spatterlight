//
//  IfdbDownloader.h
//  Spatterlight
//
//  Created by Administrator on 2019-12-11.
//

#import <Foundation/Foundation.h>

@class Metadata;

@interface IfdbDownloader : NSObject

- (instancetype)initWithContext:(NSManagedObjectContext *)context;
- (BOOL)downloadMetadataForIFID:(NSString*)ifid;
- (BOOL)downloadImageFor:(Metadata *)metadata;

@property (strong) NSManagedObjectContext *context;

@end

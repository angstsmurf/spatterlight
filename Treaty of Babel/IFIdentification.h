//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Cocoa/Cocoa.h>
#import "Metadata.h"


@interface IFIdentification : NSObject

@property(readonly) NSArray *ifids;
@property(readonly) NSString *format;
@property(readonly) NSString *bafn;
@property(readonly) Metadata *metadata;

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context;

@end

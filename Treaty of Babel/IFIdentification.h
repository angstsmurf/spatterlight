//
//  IFIdentification.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Cocoa/Cocoa.h>

@class Metadata, Ifid;

@interface IFIdentification : NSObject

@property(nonatomic) NSArray *ifids;
@property(nonatomic) NSString *format;
@property(nonatomic) NSString *bafn;
@property(nonatomic) Metadata *metadata;

@property NSManagedObjectContext *context;

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context;
- (Ifid *)fetchIfid:(NSString *)ifid;
- (Metadata *)metadataFromIfids:(NSArray<NSString *> *)ifids;

@end

//
//  IFIdentification.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Cocoa/Cocoa.h>

@class Metadata, Ifid;

@interface IFIdentification : NSObject

@property (nonatomic) NSSet *ifids;
@property (nonatomic) NSString *format;
@property (nonatomic) NSString *bafn;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;
+(void)addIfids:(NSSet<NSString *> *)ifids toMetadata:(Metadata *)metadata;
- (void)addInfoToMetadata:(Metadata *)metadata;

@end

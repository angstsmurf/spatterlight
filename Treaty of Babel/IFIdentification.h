//
//  IFIdentification.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

@class Metadata;

NS_ASSUME_NONNULL_BEGIN

@interface IFIdentification : NSObject

@property (nonatomic) NSSet<NSString *> *ifids;
@property (nonatomic, nullable) NSString *format;
@property (nonatomic, nullable) NSString *bafn;

//! Returns nil if the element contains no <ifid>
- (nullable instancetype)initWithXMLElement:(NSXMLElement *)element;
+ (void)addIfids:(NSSet<NSString *> *)ifids toMetadata:(Metadata *)metadata;
- (void)addInfoToMetadata:(Metadata *)metadata;

@end

NS_ASSUME_NONNULL_END

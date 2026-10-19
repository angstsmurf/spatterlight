//
//  IFIdentification.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface IFIdentification : NSObject

@property (nonatomic) NSSet<NSString *> *ifids;
@property (nonatomic, nullable) NSString *format;
@property (nonatomic, nullable) NSString *bafn;

//! Returns nil if the element contains no <ifid>
- (nullable instancetype)initWithXMLElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

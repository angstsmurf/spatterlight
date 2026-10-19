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

//! Identification read from the legacy Zoom metadata format, where a story
//! has one or more <id> elements with a <zcode> serial/release/checksum
//! instead of an <identification> element with IFIDs.
- (instancetype)initWithZoomIDElements:(NSArray<NSXMLElement *> *)elements;

//! Builds the IFID of a legacy Zoom <id> element the way the Babel tool does
//! for a Z-code story file without an embedded one. Returns nil if the
//! element has no serial or is not Z-code.
+ (nullable NSString *)ifidStringFromZoomIDElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

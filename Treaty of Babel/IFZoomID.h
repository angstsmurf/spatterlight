//
//  IFZoomID.h
//  Spatterlight
//
//  Created by Administrator on 2021-07-16.
//

#import "IFIdentification.h"

NS_ASSUME_NONNULL_BEGIN

//! Identification read from the legacy Zoom metadata format, where a story
//! has one or more <id> elements with a <zcode> serial/release/checksum
//! instead of an <identification> element with IFIDs.
@interface IFZoomID : IFIdentification

- (instancetype)initWithElements:(NSArray<NSXMLElement *> *)elements;

//! Builds the IFID the way the Babel tool does for a Z-code story file
//! without an embedded one. Returns nil if the element has no serial or
//! is not Z-code.
+ (nullable NSString *)ifidStringFromElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

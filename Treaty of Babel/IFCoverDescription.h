//
//  IFCoverDescription.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-31.
//

#import <Foundation/Foundation.h>

@class Metadata;

NS_ASSUME_NONNULL_BEGIN

@interface IFCoverDescription : NSObject

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@property NSString *coverArtDescription;

@end

NS_ASSUME_NONNULL_END

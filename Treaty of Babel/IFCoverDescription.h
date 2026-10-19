//
//  IFCoverDescription.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-31.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface IFCoverDescription : NSObject

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@property (nullable) NSString *coverArtDescription;

@end

NS_ASSUME_NONNULL_END

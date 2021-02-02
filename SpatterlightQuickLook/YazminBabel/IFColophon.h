//
//  IFColophon.h
//  Yazmin
//
//  Created by David Schweinsberg on 8/16/19.
//  Copyright © 2019 David Schweinsberg. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface IFColophon : NSObject

@property NSString *generator;
@property(nullable) NSString *generatorVersion;
@property NSString *originated;
@property(readonly) NSString *xmlString;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

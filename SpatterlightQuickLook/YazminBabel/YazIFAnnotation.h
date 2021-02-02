//
//  IFAnnotation.h
//  Yazmin
//
//  Created by David Schweinsberg on 8/20/19.
//  Copyright © 2019 David Schweinsberg. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class IFYazmin;

@interface IFAnnotation : NSObject

@property IFYazmin *yazmin;
@property(readonly) NSString *xmlString;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

//
//  IFIdentification.h
//  Yazmin
//
//  Created by David Schweinsberg on 25/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface IFIdentification : NSObject

@property(readonly) NSArray<NSString *> *ifids;
@property(readonly) NSString *format;
@property(readonly) int bafn;
@property(readonly) NSString *xmlString;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;
- (instancetype)initWithIFID:(NSString *)ifid;

@end

NS_ASSUME_NONNULL_END

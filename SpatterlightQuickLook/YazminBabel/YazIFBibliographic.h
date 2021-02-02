//
//  IFBibliographic.h
//  Yazmin
//
//  Created by David Schweinsberg on 25/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface IFBibliographic : NSObject

@property(nullable) NSString *title;
@property(nullable) NSString *author;
@property(nullable) NSString *language;
@property(nullable) NSString *headline;
@property(nullable) NSString *firstPublished;
@property(nullable) NSString *genre;
@property(nullable) NSString *group;
@property(nullable) NSString *storyDescription;
@property(nullable) NSString *series;
@property int seriesNumber;
@property(nullable) NSString *forgiveness;
@property(readonly) NSString *xmlString;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

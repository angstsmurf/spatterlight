//
//  IFDB.h
//  Yazmin
//
//  Created by David Schweinsberg on 8/18/19.
//  Copyright © 2019 David Schweinsberg. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface IFDB : NSObject

@property NSString *tuid;
@property NSURL *link;
@property NSURL *coverArt;
@property CGFloat averageRating;
@property CGFloat starRating;
@property NSUInteger ratingCountAvg;
@property NSUInteger ratingCountTot;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END

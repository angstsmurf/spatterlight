//
//  IFZoomID.h
//  Spatterlight
//
//  Created by Administrator on 2021-07-16.
//

#import "IFIdentification.h"

NS_ASSUME_NONNULL_BEGIN

@interface IFZoomID : IFIdentification

- (instancetype)initWithElements:(NSArray<NSXMLElement *> *)elements andContext:(NSManagedObjectContext *)context;

@end

NS_ASSUME_NONNULL_END

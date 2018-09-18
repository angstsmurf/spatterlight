//
//  Font+CoreDataClass.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-04.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class NSObject, Settings;

NS_ASSUME_NONNULL_BEGIN

@interface Font : NSManagedObject

- (Font *)clone;

@end

NS_ASSUME_NONNULL_END

#import "Font+CoreDataProperties.h"

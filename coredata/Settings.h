//
//  Settings+CoreDataClass.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-04.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

#import "Font.h"

@class Font, Game;

NS_ASSUME_NONNULL_BEGIN

@interface Settings : NSManagedObject

@property (readonly, strong) Settings * _Nonnull clone;

@end

NS_ASSUME_NONNULL_END

#import "Settings+CoreDataProperties.h"

//
//  BuiltInThemes.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-19.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Theme;

NS_ASSUME_NONNULL_BEGIN

@interface BuiltInThemes : NSObject

+ (void)createBuiltInThemesInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;

+ (nullable Theme *)createThemeFromDefaultsPlistInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;
+ (Theme *)createDefaultThemeInContext:(NSManagedObjectContext *)context forceRebuild:(BOOL)force;

@end

NS_ASSUME_NONNULL_END

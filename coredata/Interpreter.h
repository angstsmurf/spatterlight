//
//  Interpreter.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Theme;

@interface Interpreter : NSManagedObject

+ (NSFetchRequest<Interpreter *> *)fetchRequest NS_SWIFT_NAME(fetchRequest());

@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) Theme *setting;

@end

@interface Interpreter (CoreDataGeneratedAccessors)

- (void)addSettingObject:(Theme *)value;
- (void)removeSettingObject:(Theme *)value;
- (void)addSetting:(NSSet<Theme *> *)values;
- (void)removeSetting:(NSSet<Theme *> *)values;

@end

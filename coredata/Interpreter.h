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

@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) Theme *setting;

@end

//
//  Interpreter.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Game;

@interface Interpreter : NSManagedObject

@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) Game *games;

@end

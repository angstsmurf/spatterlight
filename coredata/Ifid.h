//
//  Ifid.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata, Game;

@interface Ifid : NSManagedObject

@property (nonatomic, retain) NSString * ifidString;
@property (nonatomic, retain) Metadata *metadata;

@end

//
//  Tag.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata;

@interface Tag : NSManagedObject

@property (nonatomic, retain) NSString * content;
@property (nonatomic, retain) Metadata *metadata;

@end

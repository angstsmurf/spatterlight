//
//  Tag.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata;

@interface Tag : NSManagedObject

@property (nonatomic, retain) NSString * content;
@property (nonatomic, retain) NSSet *metadata;
@end

@interface Tag (CoreDataGeneratedAccessors)

- (void)addMetadataObject:(Metadata *)value;
- (void)removeMetadataObject:(Metadata *)value;
- (void)addMetadata:(NSSet<Metadata *> *)values;
- (void)removeMetadata:(NSSet<Metadata *> *)values;

@end

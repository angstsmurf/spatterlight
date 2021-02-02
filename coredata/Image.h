//
//  Image.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata;

NS_ASSUME_NONNULL_BEGIN

@interface Image : NSManagedObject

@property (nullable, nonatomic, retain) NSObject *data;
@property (nullable, nonatomic, copy) NSString *imageDescription;
@property (nullable, nonatomic, copy) NSString *originalURL;
@property (nullable, nonatomic, retain) NSSet<Metadata *> *metadata;

@end

@interface Image (CoreDataGeneratedAccessors)

- (void)addMetadataObject:(Metadata *)value;
- (void)removeMetadataObject:(Metadata *)value;
- (void)addMetadata:(NSSet *)values;
- (void)removeMetadata:(NSSet *)values;

@end

NS_ASSUME_NONNULL_END

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

typedef NS_ENUM(int32_t, kImageInterpolationType) {
    kUnset,
    kNearestNeighbor,
    kTrilinear,
};

NS_ASSUME_NONNULL_BEGIN

//! originalURL of the single shared Image for the Inform 7 placeholder cover,
//! which is never deleted as an orphan
extern NSString * const kImagePlaceholderOriginalURL;
extern NSString * const kImagePlaceholderDescription;

@interface Image : NSManagedObject

+ (NSFetchRequest<Image *> *)fetchRequest NS_SWIFT_NAME(fetchRequest());

@property (nullable, nonatomic, retain) NSObject *data;
@property (nullable, nonatomic, copy) NSString *imageDescription;
@property (nonatomic) kImageInterpolationType interpolation;
@property (nullable, nonatomic, copy) NSString *originalURL;
@property (nullable, nonatomic, retain) NSSet<Metadata *> *metadata;

@end

@interface Image (CoreDataGeneratedAccessors)

- (void)addMetadataObject:(Metadata *)value;
- (void)removeMetadataObject:(Metadata *)value;
- (void)addMetadata:(NSSet<Metadata *> *)values;
- (void)removeMetadata:(NSSet<Metadata *> *)values;
+ (void)deleteIfOrphan:(Image *)image;

@end

// Image and Metadata are the entities MyCoreDataCoreSpotlightDelegate indexes,
// keyed by the object ID's URI. Both call this from -willSave when deleted.
@interface NSManagedObject (SpotlightRemoval)

- (void)removeFromSpotlightIndex;

@end

NS_ASSUME_NONNULL_END

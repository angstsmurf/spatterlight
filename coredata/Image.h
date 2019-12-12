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

@interface Image : NSManagedObject

@property (nonatomic, retain) id data;
@property (nonatomic, retain) NSString * originalURL;
@property (nonatomic, retain) NSString * type;
@property (nonatomic, retain) NSSet *metadata;
@end

@interface Image (CoreDataGeneratedAccessors)

- (void)addMetadataObject:(Metadata *)value;
- (void)removeMetadataObject:(Metadata *)value;
- (void)addMetadata:(NSSet *)values;
- (void)removeMetadata:(NSSet *)values;

@end

//
//  Ifid.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata;

NS_ASSUME_NONNULL_BEGIN

@interface Ifid : NSManagedObject

+ (NSFetchRequest<Ifid *> *)fetchRequest NS_SWIFT_NAME(fetchRequest());

@property (nullable, nonatomic, copy) NSString *ifidString;
@property (nullable, nonatomic, retain) Metadata *metadata;

@end

NS_ASSUME_NONNULL_END

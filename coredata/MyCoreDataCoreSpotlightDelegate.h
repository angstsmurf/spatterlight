//
//  MyCoreDataCoreSpotlightDelegate.h
//  Spatterlight
//
//  Created by Administrator on 2021-10-11.
//

#import <CoreData/CoreData.h>

NS_ASSUME_NONNULL_BEGIN

@interface MyCoreDataCoreSpotlightDelegate : NSCoreDataCoreSpotlightDelegate

//! The exporter for the store described by \c description, which must have
//! persistent history tracking on. Hides which initializer the SDK has.
+ (instancetype)exporterForStoreWithDescription:(NSPersistentStoreDescription *)description
                                    coordinator:(NSPersistentStoreCoordinator *)coordinator API_AVAILABLE(macos(10.15));

//! startSpotlightIndexing / stopSpotlightIndexing where the SDK has them,
//! otherwise nothing.
- (void)startIndexing API_AVAILABLE(macos(10.15));
- (void)stopIndexing API_AVAILABLE(macos(10.15));

@end

NS_ASSUME_NONNULL_END

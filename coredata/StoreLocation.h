//
//  StoreLocation.h
//  Spatterlight
//
//  Where the library store lives, and whether it still has to move there.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface StoreLocation : NSObject

//! Works out the store to open: the test store if CoreDataManager +testStoreURL
//! names one; else the store in the group container (or, in an unsigned build,
//! Application Support/Spatterlight); else, if only the store from before the
//! move into the group container exists and can be read, that one, flagged
//! needsMove. Creates the store folder if needed. Returns nil if there is no
//! usable folder (the error has been presented).
+ (nullable instancetype)currentLocation;

//! The store to open.
@property (readonly, nonatomic) NSURL *storeURL;
//! Where the store belongs. The same as storeURL unless needsMove.
@property (readonly, nonatomic) NSURL *destinationURL;
//! storeURL is the legacy store, to be moved to destinationURL once opened.
@property (readonly, nonatomic) BOOL needsMove;

@end

NS_ASSUME_NONNULL_END

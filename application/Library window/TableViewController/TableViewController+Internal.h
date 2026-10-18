//
//  TableViewController+Internal.h
//  Spatterlight
//
//  Internal header exposing private properties for use by
//  TableViewController categories and helper classes.
//

#import "TableViewController.h"

NS_ASSUME_NONNULL_BEGIN

@interface TableViewController ()

// Private properties previously declared as ivars
@property BOOL canEdit;
// Bumped to cancel a click-and-wait rename that has been scheduled but has
// not opened yet (a double click on the same row plays the game instead).
@property NSUInteger pendingRenameToken;
@property (nullable) NSString *searchString;
@property (nullable) NSLocale *englishUSLocale;
@property (nullable) NSDictionary *languageCodes;
@property (nullable) NSMenuItem *enabledThemeItem;

@property BOOL countingMetadataChanges;
@property NSUInteger insertedMetadataCount;
@property NSUInteger updatedMetadataCount;

@property BOOL verifyIsCancelled;
@property (nullable) NSTimer *verifyTimer;
@property (nullable) NSDictionary *forgiveness;

@end

NS_ASSUME_NONNULL_END

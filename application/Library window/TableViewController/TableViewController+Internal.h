//
//  TableViewController+Internal.h
//  Spatterlight
//
//  Internal header exposing private properties for use by
//  TableViewController categories and helper classes.
//

#import "TableViewController.h"

NS_ASSUME_NONNULL_BEGIN

//! The most info windows that Show Info opens in one go
static const NSUInteger kMaxInfoWindowsAtOnce = 20;

@interface TableViewController ()

// Private properties previously declared as ivars
@property BOOL canEdit;
// Bumped to cancel a click-and-wait rename that has been scheduled but has
// not opened yet (a double click on the same row plays the game instead).
@property NSUInteger pendingRenameToken;
@property (nullable) NSString *searchString;
@property (nullable) NSMenuItem *enabledThemeItem;

@property BOOL verifyIsCancelled;
@property (nullable) NSTimer *verifyTimer;

// Set by the first viewDidAppear; later ones only refresh the table.
@property BOOL didAppear;

//! Posts UpdateSideView with the first (at most two) selected games.
- (void)postSideViewUpdate;

@end

NS_ASSUME_NONNULL_END

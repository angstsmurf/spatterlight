/*
 * Library with metadata --
 *
 * Keep an archive of game metadata.
 * Import iFiction format from files or babel software.
 * Tag user-edited entries for export.
 * 
 */

#import <Cocoa/Cocoa.h>

@class TableViewController;

NS_ASSUME_NONNULL_BEGIN

@interface WindowViewController : NSViewController
@property (weak) IBOutlet NSProgressIndicator * _Nullable progIndicator;
@end

@interface LibController : NSWindowController <NSDraggingDestination, NSWindowDelegate, NSToolbarDelegate>

@property (strong) NSPersistentContainer * _Nullable persistentContainer;
@property (nonatomic, strong) NSManagedObjectContext * _Nullable managedObjectContext;

@property (nonatomic, strong) TableViewController * _Nullable tableViewController;

@property (nonatomic, strong) NSProgressIndicator * _Nullable progIndicator;
@property NSSearchField * _Nullable searchField;

@end

NS_ASSUME_NONNULL_END

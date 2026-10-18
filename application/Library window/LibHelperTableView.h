//
//  LibHelperTableView.h
//  Spatterlight
//
//  Extracted from TableViewController.h
//

#import <Cocoa/Cocoa.h>

@interface RatingsCellView : NSTableCellView
@property (strong) IBOutlet NSLevelIndicator *rating;
@end

@interface ForgivenessCellView : NSTableCellView
@property (strong) IBOutlet NSPopUpButton *popUpButton;
@end

@interface LikeCellView : NSTableCellView
@property (strong) IBOutlet NSButton *likeButton;
@end

@interface LibHelperTableView : NSTableView
@property NSInteger mouseOverRow;
@property IBOutlet NSBox *horizontalLine;
// The rows that were selected when the last mouse button went down, before
// the table changed the selection for that click. The click-and-wait rename
// in doClick: uses it to tell a click on the already selected row from the
// click that selected it.
@property (nullable) NSIndexSet *selectionBeforeClick;
@end

@interface MyIndicator : NSLevelIndicator
@property (weak) LibHelperTableView *tableView;
@end

@interface CellViewWithConstraint : NSTableCellView
@property (strong) IBOutlet NSLayoutConstraint *topConstraint;
@end

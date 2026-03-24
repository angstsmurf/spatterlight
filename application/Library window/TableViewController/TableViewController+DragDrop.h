//
//  TableViewController+DragDrop.h
//  Spatterlight
//
//  Drag-n-drop destination and file adding.
//

#import "TableViewController.h"

@interface TableViewController (DragDrop)

- (IBAction)addGamesToLibrary:(id)sender;
- (void)addInBackground:(NSArray<NSURL *> *)files lookForImages:(BOOL)lookForImages downloadInfo:(BOOL)downloadInfo;

@end

//
//  TableViewController+DragDrop.m
//  Spatterlight
//
//  Drag-n-drop destination and file adding.
//

#import "TableViewController+DragDrop.h"
#import "TableViewController+Internal.h"

#import "Constants.h"


#import "GameImporter.h"
#import "FolderAccess.h"

#import "CoreDataManager.h"

@implementation TableViewController (DragDrop)

#pragma mark Adding games

- (IBAction)addGamesToLibrary:(id)sender {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:YES];
    [panel setCanChooseDirectories:YES];
    panel.prompt = NSLocalizedString(@"Add", nil);

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    self.lookForCoverImagesCheckBox.state = [defaults boolForKey:@"LookForImagesWhenImporting"];
    self.downloadGameInfoCheckBox.state = [defaults boolForKey:@"DownloadInfoWhenImporting"];

    panel.accessoryView = self.downloadCheckboxView;
    // On macOS 27, the Add button opens a selected folder instead of
    // choosing it unless folders are among the allowed types.
    panel.allowedFileTypes = [gGameFileTypes arrayByAddingObject:@"public.folder"];

    TableViewController * __weak weakSelf = self;

    [panel beginSheetModalForWindow:self.view.window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSArray *urls = panel.URLs;

            BOOL lookForImages = weakSelf.lookForCoverImagesCheckBox.state == NSControlStateValueOn;
            BOOL downloadInfo = weakSelf.downloadGameInfoCheckBox.state == NSControlStateValueOn;

            [defaults setBool:lookForImages forKey:@"LookForImagesWhenImporting"];
            [defaults setBool:downloadInfo forKey:@"DownloadInfoWhenImporting"];

            [FolderAccess askForAccessToURL:urls.firstObject andThenRunBlock:^() {
                [weakSelf addInBackground:urls lookForImages:lookForImages downloadInfo:downloadInfo];
            }];
            return;
        }
    }];
}

- (void)addInBackground:(NSArray<NSURL *> *)files lookForImages:(BOOL)lookForImages downloadInfo:(BOOL)downloadInfo {

    if (self.currentlyAddingGames)
        return;

    self.downloadWasCancelled = NO;

    [self.coreDataManager saveChanges];
    [self.managedObjectContext.undoManager beginUndoGrouping];
    self.undoGroupingCount++;

    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:SpatterlightStopIndexingNotification object:nil]];

    NSManagedObjectContext *childContext = self.coreDataManager.privateChildManagedObjectContext;
    childContext.undoManager = nil;
    childContext.mergePolicy = NSMergeByPropertyStoreTrumpMergePolicy;

    self.verifyIsCancelled = YES;

    [self beginImporting];

    self.lastImageComparisonData = nil;

    self.currentlyAddingGames = YES;

    TableViewController * __weak weakSelf = self;

    [childContext performBlock:^{
        GameImporter *importer = [[GameImporter alloc] initWithTableViewController:weakSelf];
        NSDictionary *options = @{ @"context":childContext,
                                   @"lookForImages":@(lookForImages),
                                   @"downloadInfo":@(downloadInfo) };
        [importer addFiles:files options:options];
    }];
}

#pragma mark Drag-n-drop destination handler

static NSArray<NSURL *> *fileURLsOnPasteboard(NSPasteboard *pboard) {
    return [pboard readObjectsForClasses:@[NSURL.class]
                                 options:@{ NSPasteboardURLReadingFileURLsOnlyKey: @YES }] ?: @[];
}

- (NSDragOperation)draggingEntered:sender {
    NSFileManager *mgr = [NSFileManager defaultManager];
    BOOL isdir;

    if (self.currentlyAddingGames)
        return NSDragOperationNone;

    for (NSURL *url in fileURLsOnPasteboard([sender draggingPasteboard])) {
        NSString *path = url.path;
        if ([gGameFileTypes
             containsObject:path.pathExtension.lowercaseString])
            return NSDragOperationCopy;
        if ([path.pathExtension.lowercaseString isEqualToString:@"ifiction"])
            return NSDragOperationCopy;
        if ([mgr fileExistsAtPath:path isDirectory:&isdir])
            if (isdir)
                return NSDragOperationCopy;
    }

    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSArray<NSURL *> *urls = fileURLsOnPasteboard(sender.draggingPasteboard);
    if (!urls.count)
        return NO;
    [self addInBackground:urls lookForImages:NO downloadInfo:NO];
    return YES;
}

@end

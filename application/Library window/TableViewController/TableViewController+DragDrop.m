//
//  TableViewController+DragDrop.m
//  Spatterlight
//
//  Drag-n-drop destination and file adding.
//

#import "TableViewController+DragDrop.h"
#import "TableViewController+Internal.h"

#import "Constants.h"

#import "NSManagedObjectContext+safeSave.h"

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
    panel.allowedFileTypes = gGameFileTypes;

    TableViewController * __weak weakSelf = self;

    [panel beginSheetModalForWindow:self.view.window
                  completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK) {
            NSArray *urls = panel.URLs;

            BOOL lookForImages = weakSelf.lookForCoverImagesCheckBox.state ? YES : NO;
            BOOL downloadInfo = weakSelf.downloadGameInfoCheckBox.state ? YES : NO;

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
     postNotification:[NSNotification notificationWithName:@"StopIndexing" object:nil]];

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

- (NSDragOperation)draggingEntered:sender {
    extern NSString *terp_for_filename(NSString * path);

    NSFileManager *mgr = [NSFileManager defaultManager];
    BOOL isdir;
    NSUInteger i;

    if (self.currentlyAddingGames)
        return NSDragOperationNone;

    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([pboard.types containsObject:NSFilenamesPboardType]) {
        NSArray *paths = [pboard propertyListForType:NSFilenamesPboardType];
        NSUInteger count = paths.count;
        for (i = 0; i < count; i++) {
            NSString *path = paths[i];
            if ([gGameFileTypes
                 containsObject:path.pathExtension.lowercaseString])
                return NSDragOperationCopy;
            if ([path.pathExtension isEqualToString:@"iFiction"])
                return NSDragOperationCopy;
            if ([mgr fileExistsAtPath:path isDirectory:&isdir])
                if (isdir)
                    return NSDragOperationCopy;
        }
    }

    return NSDragOperationNone;
}

- (void)draggingExited:sender {
    // unhighlight window
}

- (BOOL)prepareForDragOperation:sender {
    return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard *pboard = sender.draggingPasteboard;
    if ([pboard.types containsObject:NSFilenamesPboardType]) {
        NSArray *files = [pboard propertyListForType:NSFilenamesPboardType];
        NSMutableArray *urls =
        [[NSMutableArray alloc] initWithCapacity:files.count];
        for (id tempObject in files) {
            [urls addObject:[NSURL fileURLWithPath:tempObject]];
        }
        [self addInBackground:urls lookForImages:NO downloadInfo:NO];
        return YES;
    }
    return NO;
}

@end

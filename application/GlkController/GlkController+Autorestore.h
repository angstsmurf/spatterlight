#import "GlkController.h"

@interface GlkController (Autorestore)

- (void)restoreUI;
- (void)postRestoreArrange:(id)sender;
- (GlkTextGridWindow *)findGridWindowIn:(NSView *)theView;
- (Theme *)findThemeByName:(NSString *)name;

- (NSString *)buildAppSupportDir;
- (NSString *)autosaveFileGUI;
- (NSString *)autosaveFileTerp;

- (void)dealWithOldFormatAutosaveDir;
- (void)deleteAutosaveFilesForGame:(Game *)game;
- (void)deleteAutosaveFiles;
- (void)deleteFiles:(NSArray<NSURL *> *)urls;
- (void)autoSaveOnExit;

- (void)showAutorestoreAlert:(id)userInfo;

- (IBAction)reset:(id)sender;
- (void)deferredRestart:(id)sender;
- (void)cleanup;
- (void)handleAutosave:(NSInteger)hash;

@end

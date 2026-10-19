#import "GlkController.h"

// Files in appSupportDir. The interpreter writes the library (.plist) and
// game state (.glksave) pair, plus -bak (previous pair) and -tmp (pair being
// written) variants. We write the two GUI archives.
extern NSString * const kAutosaveTerpFile;
extern NSString * const kAutosaveTerpBakFile;
extern NSString * const kAutosaveTerpTmpFile;
extern NSString * const kAutosaveGlkSaveFile;
extern NSString * const kAutosaveGlkSaveBakFile;
extern NSString * const kAutosaveGlkSaveTmpFile;
extern NSString * const kAutosaveGUIFile;
extern NSString * const kAutosaveGUILateFile;

@interface GlkController (Autorestore)

- (void)runTerpWithAutorestore;
- (void)restoreUI;
- (void)postRestoreArrange:(id)sender;
- (GlkTextGridWindow *)findGridWindowIn:(NSView *)theView;
- (Theme *)findThemeByName:(NSString *)name;

- (NSString *)gameSignature;
- (NSString *)buildAppSupportDir;
- (NSString *)autosaveFileGUI;
- (NSString *)autosaveFileTerp;
- (nullable NSString *)appSupportFile:(NSString *)name;
- (nullable NSDate *)creationDateOfFile:(NSString *)path;
- (nullable id)unarchiveAutosaveFile:(NSString *)path;

- (void)dealWithOldFormatAutosaveDir;
- (void)deleteAutosaveFilesForGame:(Game *)game;
- (void)deleteAutosaveFiles;
- (void)deleteAutosaveFilesKeepingLateGUI:(BOOL)keepLate;
- (void)deleteFiles:(NSArray<NSURL *> *)urls;
- (void)requestFinalAutosave;
- (void)autoSaveOnExit;

- (void)showAutorestoreAlert:(id)userInfo;

- (IBAction)reset:(id)sender;
- (void)deferredRestart:(id)sender;
- (void)cleanup;
- (void)handleAutosave:(NSInteger)hash;

@end

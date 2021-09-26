#import <Cocoa/Cocoa.h>

@class FolderAccess;

extern NSArray *gGameFileTypes;
extern NSArray *gDocFileTypes;
extern NSArray *gSaveFileTypes;
extern NSArray *gGridStyleNames;
extern NSArray *gBufferStyleNames;

extern NSPasteboardType PasteboardFileURLPromise,
PasteboardFilePromiseContent,
PasteboardFilePasteLocation;

extern NSDictionary *gExtMap;
extern NSDictionary *gFormatMap;
extern NSMutableDictionary<NSURL *, FolderAccess *> *globalBookmarks;

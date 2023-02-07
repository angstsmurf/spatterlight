//
//  DummyController.m
//  Spatterlight
//
//  This subclass of GlkController is used for the text style preview in the preferences panel.
//  We simply redefine all public methods to do nothing, to prevent any calls meant for running games
//  from having any effect.

#import "DummyController.h"
#include "glkimp.h"
#include "protocol.h"

@implementation DummyController

- (void)windowDidBecomeKey:(NSNotification *)notification {}
- (void)notePreferencesChanged:(NSNotification *)notification {}
- (void)noteDefaultSizeChanged:(NSNotification *)notification {}

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {
    return NO;
}

- (void)runTerp:(NSString *)terpname
       withGame:(Game *)game
          reset:(BOOL)shouldReset
     winRestore:(BOOL)windowRestoredBySystem {}

- (void)deleteAutosaveFilesForGame:(Game *)game {}

- (void)askForAccessToURL:(NSURL *)url showDialog:(BOOL)dialogFlag andThenRunBlock:(void (^)(void))block {}

- (IBAction)reset:(id)sender {}

- (void)queueEvent:(GlkEvent *)gevent {}
- (void)contentDidResize:(NSRect)frame {}
- (void)markLastSeen {}
- (void)performScroll {}
- (void)setBorderColor:(NSColor *)color fromWindow:(GlkWindow *)aWindow {}
- (void)restoreUI {}
- (void)autoSaveOnExit {}
- (void)storeScrollOffsets {}
- (void)restoreScrollOffsets {}
- (void)adjustContentView {}
- (void)cleanup {}

- (void)checkZMenu {}

- (NSArray *)accessibilityCustomActions {
    return @[];
}

- (GlkWindow *)largestWithMoves {
    return nil;
}
- (void)speakString:(NSString *)string {}

- (IBAction)speakMostRecent:(id)sender {}
- (IBAction)speakPrevious:(id)sender {}
- (IBAction)speakNext:(id)sender {}
- (IBAction)speakStatus:(id)sender {}

- (NSArray *)createCustomRotors {
    return @[];
}

- (void)forkInterpreterTask {}
- (void)deferredRestart:(id)sender {}
- (void)setBorderColor:(NSColor *)color {}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    return NO;
}

@end

#import "GlkController.h"

@interface GlkController (InterpreterGlue)

- (void)noteTaskDidTerminate:(id)sender;
- (void)queueEvent:(GlkEvent *)gevent;
- (void)noteDataAvailable:(id)sender;
- (void)noteManagedObjectContextDidChange:(NSNotification *)notification;

@end

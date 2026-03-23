#import "GlkController.h"

struct message;

@interface GlkController (GlkRequests)

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf;

- (void)handleSetTimer:(NSUInteger)millisecs;

@end

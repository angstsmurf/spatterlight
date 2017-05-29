/* Glk events to stick in the GlkController queue */

unsigned chartokeycode(unsigned ch);

@interface GlkEvent : NSObject
{
    NSInteger type;
    NSInteger win;
    NSUInteger val1, val2;
    NSString *ln;
}

- (id) initPrefsEvent;
- (id) initCharEvent: (unsigned)v forWindow: (NSInteger)name;
- (id) initLineEvent: (NSString*)v forWindow: (NSInteger)name;
- (id) initMouseEvent: (NSPoint)v forWindow: (NSInteger)name;
- (id) initTimerEvent;
- (id) initArrangeWidth: (NSInteger)aw height: (NSInteger)ah;
- (void) writeEvent: (NSInteger)fd;
- (NSInteger) type;

@end

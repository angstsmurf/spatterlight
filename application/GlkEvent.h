/* Glk events to stick in the GlkController queue */

unsigned chartokeycode(unsigned ch);

@interface GlkEvent : NSObject
{
    int type;
    int win;
    unsigned int val1, val2;
    NSString *ln;
}

- (id) initPrefsEvent;
- (id) initCharEvent: (unsigned)v forWindow: (int)name;
- (id) initLineEvent: (NSString*)v forWindow: (int)name;
- (id) initMouseEvent: (NSPoint)v forWindow: (int)name;
- (id) initTimerEvent;
- (id) initArrangeWidth: (int)aw height: (int)ah;
- (void) writeEvent: (int)fd;

@end

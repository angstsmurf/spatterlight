/* Glk events to stick in the GlkController queue */

unsigned chartokeycode(unsigned ch);

@interface GlkEvent : NSObject
{
    NSInteger type;
    NSInteger win;
    NSUInteger val1, val2;
    NSString *ln;
}

- (instancetype) initPrefsEvent NS_DESIGNATED_INITIALIZER;
- (instancetype) initCharEvent: (unsigned)v forWindow: (NSInteger)name NS_DESIGNATED_INITIALIZER;
- (instancetype) initLineEvent: (NSString*)v forWindow: (NSInteger)name NS_DESIGNATED_INITIALIZER;
- (instancetype) initMouseEvent: (NSPoint)v forWindow: (NSInteger)name NS_DESIGNATED_INITIALIZER;
- (instancetype) initTimerEvent NS_DESIGNATED_INITIALIZER;
- (instancetype) initArrangeWidth: (NSInteger)aw height: (NSInteger)ah NS_DESIGNATED_INITIALIZER;
- (void) writeEvent: (NSInteger)fd;
@property (readonly) NSInteger type;

@end

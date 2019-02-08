/* Glk events to stick in the GlkController queue */

unsigned chartokeycode(unsigned ch);

@interface GlkEvent : NSObject
{
    NSInteger type;
    NSInteger win;
    NSUInteger val1, val2;
    NSString *ln;
}

- (instancetype) initPrefsEvent;
- (instancetype) initCharEvent: (unsigned)v forWindow: (NSInteger)name;
- (instancetype) initLineEvent: (NSString*)v forWindow: (NSInteger)name;
- (instancetype) initMouseEvent: (NSPoint)v forWindow: (NSInteger)name;
- (instancetype) initTimerEvent;
- (instancetype) initArrangeWidth: (NSInteger)aw height: (NSInteger)ah;
- (instancetype) initSoundNotify: (NSInteger)notify withSound: (NSInteger)sound;
- (instancetype) initVolumeNotify: (NSInteger)notify;
- (instancetype) initLinkEvent: (NSUInteger)linkid forWindow: (NSInteger)name;

- (void) writeEvent: (NSInteger)fd;
@property (readonly) NSInteger type;

@end

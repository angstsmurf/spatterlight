/* Glk events to stick in the GlkController queue */

unsigned chartokeycode(unsigned ch);

@interface GlkEvent : NSObject {
    NSInteger win;
    NSString *ln;
    Theme *theme;
}

- (instancetype)initPrefsEvent;
- (instancetype)initCharEvent:(unsigned)v forWindow:(NSInteger)name;
- (instancetype)initLineEvent:(NSString *)v forWindow:(NSInteger)name;
- (instancetype)initMouseEvent:(NSPoint)v forWindow:(NSInteger)name;
- (instancetype)initTimerEvent;
- (instancetype)initArrangeWidth:(NSInteger)aw height:(NSInteger)ah theme:(Theme *)theme force:(BOOL)forceFlag;
- (instancetype)initSoundNotify:(NSInteger)notify withSound:(NSInteger)sound;
- (instancetype)initVolumeNotify:(NSInteger)notify;
- (instancetype)initLinkEvent:(NSUInteger)linkid forWindow:(NSInteger)name;

- (void)writeEvent:(NSInteger)fd;

@property(readonly) NSInteger type;
@property(readonly) NSUInteger val1;
@property(readonly) NSUInteger val2;
@property(readonly) BOOL forced;


@end

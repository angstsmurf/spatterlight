/* Glk events to stick in the GlkController queue */

@class Theme;

unsigned chartokeycode(unsigned ch);

@interface GlkEvent : NSObject <NSSecureCoding>

- (instancetype)initPrefsEventForTheme:(Theme *)theme;
- (instancetype)initCharEvent:(unsigned)v forWindow:(NSInteger)name;
- (instancetype)initLineEvent:(NSString *)v forWindow:(NSInteger)name terminator:(NSInteger)terminator;
- (instancetype)initMouseEvent:(NSPoint)v forWindow:(NSInteger)name;
- (instancetype)initTimerEvent;
- (instancetype)initArrangeWidth:(NSInteger)aw height:(NSInteger)ah theme:(Theme *)theme force:(BOOL)forceFlag;
- (instancetype)initSoundNotify:(NSInteger)notify withSound:(NSInteger)sound;
- (instancetype)initVolumeNotify:(NSInteger)notify;
- (instancetype)initLinkEvent:(NSUInteger)linkid forWindow:(NSInteger)name;

- (void)writeEvent:(NSInteger)fd;

@property(readonly) NSInteger type;
@property(readonly) NSInteger val1;
@property(readonly) NSInteger val2;
@property(readonly) BOOL forced;


@end

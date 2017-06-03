
@class GlkController;

@interface GlkWindow : NSView
{
    GlkController *glkctl;
    GlkStyle *styles[style_NUMSTYLES];
    NSInteger bgnd;
}

@property (readonly) NSInteger name;

- (BOOL) getStyleVal: (NSInteger)style hint: (NSInteger)hint value: (NSInteger *) value;

- (instancetype) initWithGlkController: (GlkController*)glkctl name: (NSInteger)name NS_DESIGNATED_INITIALIZER;
- (void) setStyle: (NSInteger)style windowType: (NSInteger)wintype enable: (NSInteger*)enable value:(NSInteger*)value;
@property (readonly) BOOL wantsFocus;
- (void) grabFocus;
- (void) flushDisplay;
- (void) markLastSeen;
- (void) performScroll;
- (void) makeTransparent;
- (void) setBgColor: (NSInteger)bc;
- (void) clear;
- (void) putString:(NSString*)buf style:(NSInteger)style;
- (void) moveToColumn:(NSInteger)x row:(NSInteger)y;
- (void) initLine: (NSString*)buf;
@property (readonly, copy) NSString *cancelLine;
- (void) initChar;
- (void) cancelChar;
- (void) initMouse;
- (void) cancelMouse;

- (void) fillRects: (struct fillrect *)rects count: (NSInteger)n;
- (void) drawImage: (NSImage*)buf val1: (NSInteger)v1 val2: (NSInteger)v2 width: (NSInteger)w height: (NSInteger)h;
- (void) flowBreak;

- (void) prefsDidChange;
- (void) terpDidStop;

@end

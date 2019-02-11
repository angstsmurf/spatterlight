
@class GlkController;
@class GlkHyperlink;

@interface GlkWindow : NSView
{
    GlkController *glkctl;
    GlkStyle *styles[style_NUMSTYLES];
    NSInteger bgnd;
	NSMutableArray *hyperlinks;
	GlkHyperlink *currentHyperlink;
	NSMutableDictionary *currentTerminators;

	NSInteger char_request;
}

@property (readonly) NSInteger name;

@property NSMutableDictionary *pendingTerminators;
@property BOOL terminatorsPending;
@property (readonly, copy) NSString *cancelLine;
@property (readonly) BOOL wantsFocus;

- (instancetype) initWithGlkController: (GlkController*)glkctl name: (NSInteger)name;
- (void) setStyle: (NSInteger)style windowType: (NSInteger)wintype enable: (NSInteger*)enable value:(NSInteger*)value;
- (BOOL) getStyleVal: (NSInteger)style hint: (NSInteger)hint value: (NSInteger *) value;
- (void) grabFocus;
- (void) flushDisplay;
- (void) markLastSeen;
- (void) performScroll;
- (void) makeTransparent;
- (void) setBgColor: (NSInteger)bc;
- (void) clear;
- (void) putString:(NSString*)buf style:(NSInteger)style;
- (NSDictionary *) attributesFromStylevalue: (NSInteger)stylevalue;
- (void) moveToColumn: (NSInteger)x row: (NSInteger)y;
- (void) initLine: (NSString*)buf;
- (void) initChar;
- (void) cancelChar;
- (void) initMouse;
- (void) cancelMouse;
- (void) setHyperlink: (NSUInteger)linkid;
- (void) initHyperlink;
- (void) cancelHyperlink;

- (void) fillRects: (struct fillrect *)rects count: (NSInteger)n;
- (void) drawImage: (NSImage*)buf val1: (NSInteger)v1 val2: (NSInteger)v2 width: (NSInteger)w height: (NSInteger)h;
- (void) flowBreak;
- (void) prefsDidChange;
- (void) terpDidStop;

@end

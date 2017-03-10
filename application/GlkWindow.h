
@class GlkController;

@interface GlkWindow : NSView
{
    GlkController *glkctl;
    GlkStyle *styles[style_NUMSTYLES];
    int bgnd;
    @public
    int name;
}

- (id) initWithGlkController: (GlkController*)glkctl name: (int)name;
- (void) setStyle: (int)style windowType: (int)wintype enable: (int*)enable value:(int*)value;
- (BOOL) wantsFocus;
- (void) grabFocus;
- (void) flushDisplay;
- (void) markLastSeen;
- (void) performScroll;
- (void) makeTransparent;
- (void) setBgColor: (int)bc;

- (void) clear;
- (void) putString:(NSString*)buf style:(int)style;
- (void) moveToColumn:(int)x row:(int)y;
- (void) initLine: (NSString*)buf;
- (NSString*) cancelLine;
- (void) initChar;
- (void) cancelChar;
- (void) initMouse;
- (void) cancelMouse;

- (void) fillRects: (struct fillrect *)rects count: (int)n;
- (void) drawImage: (NSImage*)buf val1: (int)v1 val2: (int)v2 width: (int)w height: (int)h;
- (void) flowBreak;

- (void) prefsDidChange;
- (void) terpDidStop;

@end

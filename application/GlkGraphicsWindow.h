
@interface GlkGraphicsWindow : GlkWindow {
    NSImage *image;

    NSInteger bgnd;
    
    BOOL dirty;
    BOOL mouse_request;
    BOOL transparent;
}

@end

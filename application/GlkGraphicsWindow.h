
@interface GlkGraphicsWindow : GlkWindow
{
    NSImage *image;
    BOOL dirty;
    BOOL mouse_request;
    BOOL transparent;
}

@end

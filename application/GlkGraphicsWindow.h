
@interface GlkGraphicsWindow : GlkWindow
{
    NSImage *image;
    BOOL dirty;
    NSInteger mouse_request;
    NSInteger transparent;
}

@end

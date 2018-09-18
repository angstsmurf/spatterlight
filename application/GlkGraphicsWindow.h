
@interface GlkGraphicsWindow : GlkWindow
{
    NSImage *image;
    BOOL dirty;
    NSInteger char_request;
    NSInteger mouse_request;
    NSInteger transparent;
}

@end

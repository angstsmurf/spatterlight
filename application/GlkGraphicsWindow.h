
@interface GlkGraphicsWindow : GlkWindow
{
    NSImage *image;
    BOOL dirty;
    BOOL background_color_unset;
    NSInteger mouse_request;
    NSInteger transparent;
}

@end

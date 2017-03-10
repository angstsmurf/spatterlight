
@interface GlkGraphicsWindow : GlkWindow
{
    NSImage *image;
    BOOL dirty;
    int char_request;
    int mouse_request;
    int transparent;
}

@end

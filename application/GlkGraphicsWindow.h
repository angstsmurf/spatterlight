
@interface GlkGraphicsWindow : GlkWindow <NSSecureCoding> {
    NSImage *image;
    
    BOOL mouse_request;
    BOOL transparent;
}

@end

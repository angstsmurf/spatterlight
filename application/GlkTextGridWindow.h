/* TextGrid window controller */

@interface GlkTextGridWindow : GlkWindow
{
    NSMutableArray *lines;
    NSTextField *input;
    NSInteger rows, cols;
    NSInteger xpos, ypos;
    NSInteger line_request;
    NSInteger char_request;
    NSInteger mouse_request;
    BOOL dirty;
    NSInteger transparent;
}

@end

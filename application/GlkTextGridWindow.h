/* TextGrid window controller */

@interface GlkTextGridWindow : GlkWindow
{
    NSMutableArray *lines;
    NSTextField *input;
    int rows, cols;
    int xpos, ypos;
    int line_request;
    int char_request;
    int mouse_request;
    BOOL dirty;
    int transparent;
}

@end

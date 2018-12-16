/* TextGrid window controller */

@interface GlkTextGridWindow : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate>
{
    //NSMutableArray *lines;
    NSTextField *input;
	NSTextView *textview;
	NSTextStorage *textstorage;
	NSLayoutManager *layoutmanager;
	NSTextContainer *container;
    NSInteger rows, cols;
    NSInteger xpos, ypos;
    NSInteger line_request;
	NSInteger hyper_request;
    NSInteger mouse_request;
    BOOL dirty;
    NSInteger transparent;
}

@end

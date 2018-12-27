/* TextGrid window controller */

@interface GlkTextGridWindow : GlkWindow <NSTextViewDelegate, NSTextStorageDelegate>
{
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

- (BOOL) myMouseDown: (NSEvent*)theEvent;
- (IBAction)speakStatus:(id)sender;

@end

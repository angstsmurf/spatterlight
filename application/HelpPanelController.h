//
//  HelpPanelController.h
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//

@interface HelpTextView : NSTextView <NSTextFinderClient> {
    NSTextFinder *_textFinder; // define your own text finder
}

@property(readonly) NSTextFinder *textFinder;
@end

@interface HelpPanelController : NSWindowController

@property IBOutlet HelpTextView *textView;
@property IBOutlet NSScrollView *scrollView;

- (void)showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title;

- (IBAction)copyButton:(id)sender;

@end

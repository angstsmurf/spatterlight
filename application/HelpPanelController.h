//
//  HelpPanelController.h
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//

#import <Cocoa/Cocoa.h>

@interface HelpTextView : NSTextView <NSTextFinderClient>
{
    NSTextFinder* _textFinder; // define your own text finder
}

@property (weak, readonly) NSTextFinder* textFinder;

@end

@interface HelpPanelController : NSWindowController
{
    IBOutlet HelpTextView *textView;
    IBOutlet NSScrollView *scrollView;
}

- (void) showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title;

- (IBAction)copyButton:(id)sender;

@end
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

@property (readonly) NSTextFinder* textFinder;

@end

@interface HelpPanelController : NSWindowController

@property (strong) IBOutlet HelpTextView *textView;
@property (strong) IBOutlet NSScrollView *scrollView;

- (void) showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title;

- (IBAction)copyButton:(id)sender;

@end
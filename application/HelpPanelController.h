//
//  HelpPanelController.h
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//

#import <Cocoa/Cocoa.h>

@interface HelpPanelController : NSWindowController

@property (strong) IBOutlet NSTextView *textView;
@property (strong) IBOutlet NSScrollView *scrollView;

- (void) showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title;

- (IBAction)copyButton:(id)sender;

@end

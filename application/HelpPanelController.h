//
//  HelpPanelController.h
//  Spatterlight
//
//  Created by Administrator on 2017-06-23.
//
//


@class ScalingScrollView;

@interface HelpTextView : NSTextView <NSTextFinderClient>

@property(readonly) NSTextFinder *textFinder;
@end

@interface HelpPanelController : NSWindowController

@property IBOutlet HelpTextView *textView;
@property IBOutlet ScalingScrollView *scrollView;

- (void)showHelpFile:(NSAttributedString *)text withTitle:(NSString *)title;

- (IBAction)copyButton:(id)sender;

@end

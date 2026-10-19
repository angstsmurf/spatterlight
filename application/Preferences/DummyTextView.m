#import "DummyTextView.h"
#import "Preferences.h"

@implementation DummyTextView

- (void)updateTextWithAttributes:(NSDictionary *)attributes {
    NSAttributedString *attStr = [[NSAttributedString alloc] initWithString:NSLocalizedString(@"ABCabc1234fiffiflfflct", nil) attributes:attributes];
    [self.textStorage setAttributedString:attStr];
    self.selectedRange = NSMakeRange(0, self.string.length);
}

// These three are sent from the font panel

- (void)changeFont:(id)fontManager {
    [super changeFont:fontManager];
    _sender = fontManager;
    [[Preferences instance] changeFont:self];
}

- (void)changeAttributes:(id)sender {
    [super changeAttributes:sender];
    _sender = sender;
    [[Preferences instance] changeAttributes:self];
}

- (void)changeDocumentBackgroundColor:(id)sender {
    [super changeDocumentBackgroundColor:sender];
    _sender = sender;
    [[Preferences instance] changeDocumentBackgroundColor:self];
}

@end

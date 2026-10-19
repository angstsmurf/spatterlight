//
//  FindableTextView.m
//  Spatterlight
//

#import "FindableTextView.h"

@interface FindableTextView () {
    NSTextFinder *_textFinder;
}
@end

@implementation FindableTextView

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (menuItem.action == @selector(performTextFinderAction:))
        return [self.textFinder validateAction:menuItem.tag];
    return [super validateMenuItem:menuItem];
}

- (NSTextFinder *)textFinder {
    if (_textFinder == nil) {
        _textFinder = [[NSTextFinder alloc] init];
        _textFinder.client = self;
        _textFinder.findBarContainer = self.enclosingScrollView;
        _textFinder.incrementalSearchingEnabled = YES;
        _textFinder.incrementalSearchingShouldDimContentView = NO;
    }

    return _textFinder;
}

- (void)resetTextFinder {
    [_textFinder noteClientStringWillChange];
}

- (void)performTextFinderAction:(id<NSValidatedUserInterfaceItem>)sender {
    [self.textFinder performAction:sender.tag];
}

@end

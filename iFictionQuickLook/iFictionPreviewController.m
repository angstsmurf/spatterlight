//
//  iFictionPreviewController.m
//  iFictionQuickLook
//
//  Created by Administrator on 2021-02-15.
//

#import "iFictionPreviewController.h"
#import "UKSyntaxColor.h"

@interface iFictionPreviewController ()
    
@end

@implementation iFictionPreviewController

- (void)loadView {
    [super loadView];
    if (@available(macOS 11, *)) {
        self.preferredContentSize = NSMakeSize(820, 846);
    }
}

/*
 * Implement this method and set QLSupportsSearchableItems to YES in the Info.plist of the extension if you support CoreSpotlight.
 *
- (void)preparePreviewOfSearchableItemWithIdentifier:(NSString *)identifier queryString:(NSString *)queryString completionHandler:(void (^)(NSError * _Nullable))handler {
    
    // Perform any setup necessary in order to prepare the view.
    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.

    handler(nil);
}
*/

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {

    if (@available(macOS 10.15, *)) {
        NSError *error = nil;
        NSXMLDocument *xml =
        [[NSXMLDocument alloc] initWithContentsOfURL:url options: NSXMLDocumentTidyXML error:&error];

        if (error)
            NSLog(@"Error: %@", error);

        NSString *contents = [xml XMLStringWithOptions:NSXMLNodePrettyPrint];
        if (!contents || !contents.length) {
            contents = @"<No iFiction data found in file>";
        }

        NSBundle *main = [NSBundle mainBundle];
        NSString *resourcePath = [main pathForResource:@"iFiction" ofType:@"plist"];

        NSURL *plisturl = [NSURL fileURLWithPath:resourcePath isDirectory:NO];

        _syntaxColorer = [[UKSyntaxColor alloc] initWithString:contents];

        error = nil;

        _syntaxColorer.syntaxDefinitionDictionary = [NSDictionary dictionaryWithContentsOfURL:plisturl error:&error];


        if (error)
            NSLog(@"Error: %@", error);

        NSMutableDictionary *defaultText = [NSMutableDictionary new];

        NSMutableParagraphStyle *style;

        defaultText[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize] weight:NSFontWeightRegular];
        style = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        style.firstLineHeadIndent = 0;
        style.headIndent = 60;
        defaultText[NSParagraphStyleAttributeName] = style;

        _syntaxColorer.defaultTextAttributes = defaultText;

        [_syntaxColorer recolorCompleteFile:nil];

        [_textview.textStorage setAttributedString:_syntaxColorer.coloredString];
    }

    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.
    handler(nil);
}

@end


//
//  iFictionPreviewController.m
//  iFictionQuickLook
//
//  Created by Administrator on 2021-02-15.
//
#import <Quartz/Quartz.h>

#import "iFictionPreviewController.h"
#import "UKSyntaxColor.h"

@interface iFictionPreviewController () <QLPreviewingController>
    
@end

@implementation iFictionPreviewController

- (NSString *)nibName {
    return @"iFictionPreviewController";
}

- (void)loadView {
    [super loadView];
    // Do any additional setup after loading the view.
//    self.preferredContentSize = NSMakeSize(820, 846);
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

    // Add the supported content types to the QLSupportedContentTypes array in the Info.plist of the extension.
    
    // Perform any setup necessary in order to prepare the view.

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

    NSURL *plisturl = [NSURL fileURLWithPath:resourcePath];

    _syntaxColorer = [[UKSyntaxColor alloc] initWithString:contents];

    error = nil;

    if (@available(macOS 10.13, *)) {
        _syntaxColorer.syntaxDefinitionDictionary = [NSDictionary dictionaryWithContentsOfURL:plisturl error:&error];
    }

    if (error)
        NSLog(@"Error: %@", error);

    NSMutableDictionary *defaultText = [NSMutableDictionary new];

    NSMutableParagraphStyle *style;

    if (@available(macOS 10.15, *)) {
        defaultText[NSFontAttributeName] = [NSFont systemFontOfSize:[NSFont systemFontSize] weight:NSFontWeightRegular];
        style = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        style.firstLineHeadIndent = 0;
        style.headIndent = 60;
        defaultText[NSParagraphStyleAttributeName] = style;
    }

    _syntaxColorer.defaultTextAttributes = defaultText;

    [_syntaxColorer recolorCompleteFile:nil];

    [_textview.textStorage setAttributedString:_syntaxColorer.coloredString];

    
    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.
    
    handler(nil);
}

@end


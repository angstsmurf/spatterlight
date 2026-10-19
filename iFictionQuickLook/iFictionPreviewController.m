//
//  iFictionPreviewController.m
//  iFictionQuickLook
//
//  Created by Administrator on 2021-02-15.
//

#import "iFictionPreviewController.h"

// Each rule colours every match of its pattern with a colour from
// Quicklook.xcassets. Rules run in order and a later rule overrides an
// earlier one, so e.g. a quoted attribute value inside a tag gets the
// string colour, and anything inside a comment gets the comment colour.
// Text no rule matches keeps the text colour.
typedef struct {
    NSString *pattern;
    NSString *colorName;
} iFictionColorRule;

static const iFictionColorRule kColorRules[] = {
    // Tags. An unterminated tag runs to the end of the text.
    { @"<[^>]*>?", @"tagColor" },
    // Quoted strings. XML has no backslash escapes.
    { @"\"[^\"]*\"", @"stringColor" },
    // Entity and character references.
    { @"&#?[a-z0-9]+;", @"symbolColor" },
    { @"</?story>", @"stringColor" },
    // Restores the tag colour of line breaks inside quoted text.
    { @"</?br>", @"tagColor" },
    // Comments. An unterminated comment runs to the end of the text.
    { @"<!--.*?(?:-->|\\z)", @"customControlColor" },
};

static const CGFloat kWrappedLineIndent = 60;

@implementation iFictionPreviewController

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler {
    NSError *error = nil;
    NSXMLDocument *xml = [[NSXMLDocument alloc] initWithContentsOfURL:url options:NSXMLDocumentTidyXML error:&error];
    if (!xml) {
        handler(error);
        return;
    }

    NSString *contents = [xml XMLStringWithOptions:NSXMLNodePrettyPrint];
    if (!contents.length) {
        contents = @"<No iFiction data found in file>";
    }

    NSMutableParagraphStyle *style = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
    style.firstLineHeadIndent = 0;
    style.headIndent = kWrappedLineIndent;

    NSDictionary *attributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:[NSFont systemFontSize] weight:NSFontWeightRegular],
        NSParagraphStyleAttributeName: style
    };

    [_textview.textStorage setAttributedString:[iFictionPreviewController coloredXMLString:contents attributes:attributes]];

    // Call the completion handler so Quick Look knows that the preview is fully loaded.
    // Quick Look will display a loading spinner while the completion handler is not called.
    handler(nil);
}

+ (NSAttributedString *)coloredXMLString:(NSString *)xml attributes:(NSDictionary<NSAttributedStringKey, id> *)attributes {
    NSMutableDictionary *baseAttributes = [attributes mutableCopy];
    baseAttributes[NSForegroundColorAttributeName] = [NSColor textColor];

    NSMutableAttributedString *colored = [[NSMutableAttributedString alloc] initWithString:xml attributes:baseAttributes];
    NSRange all = NSMakeRange(0, colored.length);

    for (size_t i = 0; i < sizeof(kColorRules) / sizeof(kColorRules[0]); i++) {
        NSError *error = nil;
        NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:kColorRules[i].pattern options:NSRegularExpressionCaseInsensitive | NSRegularExpressionDotMatchesLineSeparators error:&error];
        if (!regex) {
            NSLog(@"iFictionPreviewController: bad pattern %@: %@", kColorRules[i].pattern, error);
            continue;
        }
        NSColor *color = [NSColor colorNamed:kColorRules[i].colorName];
        if (!color) {
            color = [NSColor textColor];
        }
        [regex enumerateMatchesInString:xml options:0 range:all usingBlock:^(NSTextCheckingResult *match, NSMatchingFlags flags, BOOL *stop) {
            [colored addAttribute:NSForegroundColorAttributeName value:color range:match.range];
        }];
    }

    // Make sure Japanese etc. fallback fonts get applied.
    [colored fixFontAttributeInRange:all];
    return colored;
}

@end

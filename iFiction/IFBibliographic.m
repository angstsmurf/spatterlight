//
//  IFBibliographic.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFBibliographic.h"
#import "NSString+Categories.h"

// The values stored in Metadata.forgivenessNumeric. Must match the
// forgiveness pop-up in the library window.
static NSDictionary<NSString *, NSNumber *> *ForgivenessLevels(void) {
    static NSDictionary<NSString *, NSNumber *> *levels;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        levels = @{
            @"" : @0,
            @"Cruel" : @1,
            @"Nasty" : @2,
            @"Tough" : @3,
            @"Polite" : @4,
            @"Merciful" : @5
        };
    });
    return levels;
}

// Converts the contents of an iFiction <language> element to a language code
// and, if the code is known, its English name.
//
// In IFDB xml data, "language" is usually a two-letter ISO 639 code such
// as "en", but may also be a locale code such as "en-US" or a descriptive
// string like "English, French (en, fr)."
// We keep the English name on purpose: it is what the library sorts and
// searches on, whatever the user's locale.
NSString *IFLanguageCodeFromString(NSString *raw, NSString * _Nullable * _Nullable languageAsWord) {
    static NSLocale *englishUSLocale;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        englishUSLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
    });

    NSString *languageCode = raw;
    NSString *word = nil;
    if (languageCode.length > 1) {
        // A descriptive string lists the codes in parentheses, main one first
        NSRange open = [raw rangeOfString:@"("];
        NSRange close = [raw rangeOfString:@")" options:NSBackwardsSearch];
        if (open.location != NSNotFound && close.location != NSNotFound && close.location > open.location) {
            NSString *inside = [raw substringWithRange:NSMakeRange(NSMaxRange(open), close.location - NSMaxRange(open))];
            NSCharacterSet *separators = [NSCharacterSet characterSetWithCharactersInString:@", "];
            for (NSString *code in [inside componentsSeparatedByCharactersInSet:separators]) {
                if (code.length) {
                    languageCode = code;
                    break;
                }
            }
        }
        if (languageCode.length > 3) {
            // If it is longer than three characters, we use the first two
            // as language code. This covers locale codes such as "en-US".
            languageCode = [languageCode substringToIndex:2];
        }
        languageCode = languageCode.lowercaseString;
        word = [englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
        if (!word) {
            // Otherwise we use the full string for both language and languageAsWord.
            word = raw;
            languageCode = raw;
        }
    }
    if (languageAsWord)
        *languageAsWord = word;
    return languageCode;
}

// Joins the text nodes of a <description> element with blank lines and
// cleans up the ways line breaks are spelled in the wild.
static NSString *BlurbFromDescriptionElement(NSXMLElement *element) {
    NSMutableArray<NSString *> *paragraphs = [NSMutableArray new];
    for (NSXMLNode *node in element.children) {
        if (node.kind == NSXMLTextKind)
            [paragraphs addObject:node.stringValue];
    }

    NSString *blurb = [paragraphs componentsJoinedByString:@"\n\n"].stringByDecodingXMLEntities;

    // Descriptions that escaped their line breaks, such as the one in the
    // Grandpa's Ranch blorb ("&lt;br&gt;"), still spell the tags out at this
    // point, so replace them with actual newlines.
    blurb = blurb.stringByReplacingBreakTagsWithNewlines;

    // When importing Return to Ditch Day, Babel returns a text with \\n instead of line breaks.
    blurb = [blurb stringByReplacingOccurrencesOfString:@"\\n" withString:@"\n"];

    return [blurb stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
}

@implementation IFBibliographic

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        for (NSXMLNode *node in element.children) {
            NSString *keyVal = node.stringValue;
            NSString *key = node.name;
            if (!key)
                continue;

            if ([key isEqualToString:@"firstpublished"] || [key isEqualToString:@"year"]) {
                [self parseFirstPublished:keyVal];
            } else if ([key isEqualToString:@"language"]) {
                NSString *word = nil;
                _language = IFLanguageCodeFromString(keyVal, &word);
                if (word)
                    _languageAsWord = word;
            } else if ([key isEqualToString:@"forgiveness"]) {
                _forgivenessNumeric = ForgivenessLevels()[keyVal];
                if (_forgivenessNumeric.integerValue > 0)
                    _forgiveness = keyVal;
            } else if ([key isEqualToString:@"title"]) {
                NSString *title = keyVal.stringByDecodingXMLEntities;
                if (title.length)
                    _title = title;
            } else if ([key isEqualToString:@"headline"]) {
                _headline = [keyVal stringByReplacingOccurrencesOfString:@"[em dash]" withString:@"-"];
            } else if ([key isEqualToString:@"author"]) {
                _author = keyVal;
            } else if ([key isEqualToString:@"genre"]) {
                _genre = keyVal;
            } else if ([key isEqualToString:@"group"]) {
                _group = keyVal;
            } else if ([key isEqualToString:@"series"]) {
                _series = keyVal;
            } else if ([key isEqualToString:@"seriesnumber"]) {
                _seriesnumber = keyVal;
            } else if ([key isEqualToString:@"description"] || [key isEqualToString:@"teaser"]) {
                _blurb = BlurbFromDescriptionElement((NSXMLElement *)node);
            }
        }
    }
    return self;
}

// The Treaty of Babel allows "YYYY" or "YYYY-MM-DD". Anything else is ignored.
- (void)parseFirstPublished:(NSString *)string {
    static NSDateFormatter *yearFormatter, *dayFormatter;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSLocale *posix = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"];
        NSTimeZone *utc = [NSTimeZone timeZoneForSecondsFromGMT:0];
        yearFormatter = [NSDateFormatter new];
        yearFormatter.locale = posix;
        yearFormatter.timeZone = utc;
        yearFormatter.dateFormat = @"yyyy";
        dayFormatter = [NSDateFormatter new];
        dayFormatter.locale = posix;
        dayFormatter.timeZone = utc;
        dayFormatter.dateFormat = @"yyyy-MM-dd";
    });

    NSDate *date = nil;
    if (string.length == 4)
        date = [yearFormatter dateFromString:string];
    else if (string.length == 10)
        date = [dayFormatter dateFromString:string];

    if (!date) {
        NSLog(@"IFBibliographic: illegal date format \"%@\"", string);
        return;
    }
    _firstPublishedDate = date;
    _firstPublished = [yearFormatter stringFromDate:date];
}

@end
